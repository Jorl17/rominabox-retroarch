#pragma once
#include "document_contract.hpp"

#include <string>
#include <cstring>
#include "events.h"
#include <retro_miscellaneous.h>

namespace rib {
static inline bool state_task_matches(
      bool pending, bool pending_is_save, const char *pending_path,
      int pending_slot, const char *path, int slot, bool is_save)
{
   if (!pending || pending_is_save != is_save)
      return false;
   if (path && path[0] && pending_path && pending_path[0]
         && strcmp(pending_path, path) != 0)
      return false;
   if (slot >= 0 && pending_slot >= 0 && slot != pending_slot)
      return false;
   return true;
}


using document_contract::kSlotCount;
inline bool valid_slot(int slot) { return slot >= 1 && slot <= kSlotCount; }

class Document;
class Focus;
class Status;

/* The selected slot, kept thumbnails, checks and the current save or load
 * task. We store through the host, and match each callback to its request. */
class Slots
{
public:
   Slots(Document& document, Focus& focus, Status& status)
      : document(document), focus(focus), status(status) {}
   enum class Transfer { Save, Load };
   void reset_transfer() { transfer = {}; }
   bool transfer_pending() const { return transfer.pending; }
   bool load_available() const;
   void refresh();
   void request(Transfer kind);
   void notify_task(const char *path, int slot, bool is_save, bool success);
   void focus_action(const Event& event);
   void focus_element(const char *id);
   void paint() const;
   void set_selected_slot(int slot);
   void set_slot_state(int slot, bool occupied, const char *thumbnail_path);
   void set_game_aspect(float aspect);
   void guard_slots(const char *label, const char *reason);
   bool slots_guarded() const { return !guard.empty(); }
   int selected() const { return selected_slot; }
   bool occupied(int slot) const;
   bool has_thumbnail(int slot) const;

private:
   bool begin_transfer(Transfer kind);
   struct Request
   {
      bool pending = false;
      Transfer kind = Transfer::Load;
      int slot = 0;
      char path[PATH_MAX_LENGTH]{};
   } transfer;
   struct SlotState
   {
      bool occupied = false;
      std::string thumbnail_path;
      std::string thumbnail_version;
   };
   static std::string thumbnail_version(const std::string& path);
   static bool thumbnail_ready(const std::string& path);
   static std::string quoted_css_path(const std::string& path);
   Document& document;
   Focus& focus;
   Status& status;
   float game_aspect = 4.0f / 3.0f;
   int selected_slot = 1;
   SlotState slots[kSlotCount];
   std::string guard;
   std::string guard_reason;
};
}
