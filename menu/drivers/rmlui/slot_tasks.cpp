#include "words.hpp"
#include "slots.hpp"
#include "host.h"
#include "status.hpp"
#include "focus.hpp"
#include "sounds.hpp"
#include <cstdio>

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

void Slots::follow()
{
   if (!awaiting.slot)
      return;
   look_at(awaiting.slot);
   if (has_thumbnail(awaiting.slot) || rib_host_time_us() >= awaiting.until)
      awaiting = {};
}

bool Slots::begin_transfer(Transfer kind)
{
   if (transfer.pending) return false;
   transfer.path[0] = '\0';
   if (!rib_host_state_path(selected_slot, transfer.path, sizeof(transfer.path)))
      transfer.path[0] = '\0';
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
   char message[64];
   const bool save = kind == Transfer::Save;
   std::snprintf(message, sizeof(message),
         save ? rib::words::SavingSlot : rib::words::LoadingSlot, selected_slot);
   status.set_main(message);
   const bool accepted = save ? rib_host_save_state() : rib_host_load_state();
   // With a synchronous host callback, the request may already be complete.
   if (!accepted && transfer.pending)
      notify_task(transfer.path, transfer.slot, save, false);
}

void Slots::notify_task(const char *path, int slot, bool is_save, bool success)
{
   if (!state_task_matches(transfer.pending,
         transfer.kind == Transfer::Save, transfer.path, transfer.slot,
         path, slot, is_save)) return;
   transfer.pending = false;
   /* Read the slots after the task. The picture of a save comes after the
    * state, so we follow that slot until its picture arrives. */
   refresh();
   if (success && is_save && !has_thumbnail(transfer.slot))
      awaiting = {transfer.slot, rib_host_time_us() + kPictureWaitUs};
   char message[64];
   if (success)
      std::snprintf(message, sizeof(message),
            is_save ? rib::words::SavedSlot : rib::words::LoadedSlot, transfer.slot);
   else
      std::snprintf(message, sizeof(message), is_save ? rib::words::SaveFailed : rib::words::LoadFailed);
   status.set_main(message);
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
