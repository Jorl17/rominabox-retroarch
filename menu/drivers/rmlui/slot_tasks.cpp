#include "words.hpp"
#include "slots.hpp"
#include "host.h"
#include "status.hpp"
#include "focus.hpp"
#include "sounds.hpp"
#include <cstdio>
#include <string>

namespace rib {
bool Slots::load_available() const
{
   return rib_host_slot_occupied(selected_slot);
}

void Slots::look_at(int slot)
{
   char thumbnail_path[PATH_MAX_LENGTH] = {};
   const bool occupied = rib_host_slot_occupied(slot);
   rib_host_thumbnail(slot, thumbnail_path, sizeof(thumbnail_path));
   set_slot_state(slot, occupied, thumbnail_path);
}

void Slots::refresh()
{
   set_game_aspect(rib_host_game_aspect());
   for (int slot = 1; slot <= kSlotCount; ++slot)
      look_at(slot);
}

std::string Slots::shown_picture(int slot) const
{
   const SlotState& state = slots[slot - 1];
   return state.thumbnail_path + '\n' + state.thumbnail_version;
}

void Slots::follow()
{
   if (!awaiting.slot)
      return;
   look_at(awaiting.slot);
   if (shown_picture(awaiting.slot) != awaiting.reported
         || rib_host_time_us() >= awaiting.until)
      awaiting = {};
}

bool Slots::begin_transfer(Transfer kind)
{
   if (transfer.pending) return false;
   char path[PATH_MAX_LENGTH];
   transfer.path = rib_host_state_path(selected_slot, path, sizeof(path)) ? path : "";
   transfer.kind = kind;
   transfer.slot = selected_slot;
   transfer.pending = true;
   return true;
}

void Slots::request(Transfer kind)
{
   if (kind == Transfer::Load && !load_available()) return;
   if (!begin_transfer(kind)) return;
   rib_host_select_state_slot(selected_slot);
   const bool save = kind == Transfer::Save;
   status.set_main(say(save ? Word::SavingSlot : Word::LoadingSlot,
         {{"slot", std::to_string(selected_slot)}}).c_str());
   const bool accepted = save ? rib_host_save_state() : rib_host_load_state();
   // With a synchronous host callback, the request may already be complete.
   if (!accepted && transfer.pending)
      notify_task(transfer.path.c_str(), transfer.slot, save, false);
}

void Slots::notify_task(const char *path, int slot, bool is_save, bool success)
{
   if (!state_task_matches(transfer.pending,
         transfer.kind == Transfer::Save, transfer.path.c_str(), transfer.slot,
         path, slot, is_save)) return;
   transfer.pending = false;
   /* Read the slots after the task. The picture of a save comes after the
    * state, so we follow that slot until its new picture arrives, in place of
    * the picture of the previous save, if any. */
   refresh();
   if (success && is_save && valid_slot(transfer.slot))
      awaiting = {transfer.slot, rib_host_time_us() + kPictureWaitUs,
            shown_picture(transfer.slot)};
   if (success)
      status.set_main(say(is_save ? Word::SavedSlot : Word::LoadedSlot,
            {{"slot", std::to_string(transfer.slot)}}).c_str());
   else
      status.set_main(say(is_save ? Word::SaveFailed : Word::LoadFailed).c_str());
}

bool Slots::handle(const Event& event)
{
   switch (event.kind)
   {
      case RIB_RMLUI_ACTION_SAVE:
      case RIB_RMLUI_ACTION_LOAD:
      {
         const Transfer kind = event.kind == RIB_RMLUI_ACTION_SAVE ? Transfer::Save : Transfer::Load;
         if (transfer_pending() || (kind == Transfer::Load && !load_available()))
            return true;
         play_action_sound(event.kind);
         request(kind);
         return true;
      }
      case RIB_RMLUI_ACTION_SELECT_SLOT:
         if (valid_slot(event.slot))
         {
            set_selected_slot(event.slot);
            focus.set((document_contract::Slot + std::to_string(event.slot)).c_str());
         }
         return true;
      default:
         return false;
   }
}
}
