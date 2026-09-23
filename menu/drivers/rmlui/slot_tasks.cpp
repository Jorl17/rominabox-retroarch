#include "slots.hpp"
#include "host.h"
#include "status.hpp"
#include "../rmlui_bridge.h"
#include <cstdio>

namespace rib {
bool Slots::load_available() const
{
   return rib_host_slot_occupied(selected_slot);
}

void Slots::refresh()
{
   for (int slot = 1; slot <= kSlotCount; ++slot)
   {
      char thumbnail_path[PATH_MAX_LENGTH] = {};
      const bool occupied = rib_host_slot_occupied(slot);
      rib_host_thumbnail(slot, thumbnail_path, sizeof(thumbnail_path));
      set_game_aspect(rib_host_game_aspect());
      set_slot_state(slot, occupied, thumbnail_path);
   }
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
         save ? "SAVING SLOT %d..." : "LOADING SLOT %d...", selected_slot);
   status.set_main(message);
   const bool accepted = save ? rib_host_save_state() : rib_host_load_state();
   // With a synchronous host callback, the request may already be complete.
   if (!accepted && transfer.pending)
      notify_task(transfer.path, transfer.slot, save, false);
}

void Slots::notify_task(const char *path, int slot, bool is_save, bool success)
{
   if (!rib_rmlui_state_task_matches(transfer.pending,
         transfer.kind == Transfer::Save, transfer.path, transfer.slot,
         path, slot, is_save)) return;
   transfer.pending = false;
   char message[64];
   if (success)
      std::snprintf(message, sizeof(message),
            is_save ? "SLOT %d SAVED" : "SLOT %d LOADED", transfer.slot);
   else
      std::snprintf(message, sizeof(message), is_save ? "SAVE FAILED" : "LOAD FAILED");
   status.set_main(message);
}
}
