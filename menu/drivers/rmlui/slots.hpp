#pragma once

#include <string>

namespace rib {
inline constexpr int kSlotCount = 6;
inline bool valid_slot(int slot) { return slot >= 1 && slot <= kSlotCount; }

class Document;
class Focus;
class Status;

/* The six save slots on screen and the thumbnail data we keep for them. The
 * storage and the save and load tasks belong to the menu and the host. */
class Slots
{
public:
   Slots(Document& document, Focus& focus, Status& status)
      : document(document), focus(&focus), status(status) {}
   /* A new menu has a new Focus, but we keep this display state across new
    * documents and new menus. */
   void bind_focus(Focus& next) { focus = &next; }
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
   Focus *focus;
   Status& status;
   float game_aspect = 4.0f / 3.0f;
   int selected_slot = 1;
   SlotState slots[kSlotCount];
   std::string guard;
   std::string guard_reason;
};
}
