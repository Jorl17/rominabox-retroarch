#pragma once
#include "document_contract.hpp"

#include "events.h"
#include <array>
#include <vector>

namespace rib {
static inline enum rib_rmlui_action map_menu_toggle(
      bool controls_visible, bool capture_active)
{
   if (capture_active)
      return RIB_RMLUI_ACTION_CONTROLS_CANCEL;
   if (controls_visible)
      return RIB_RMLUI_ACTION_CONTROLS_BACK;
   return RIB_RMLUI_ACTION_RESUME;
}

static inline bool toggle_stays_in_menu(
      bool controls_visible, bool capture_active)
{
   return capture_active || controls_visible;
}


enum class FocusRegion { Pause, Controls, List, Parts };

struct FocusTarget
{
   enum class Kind { Item, Reset, Back, Slots };
   Kind kind = Kind::Item;
   int index = 0;
   static FocusTarget item(int index) { return {Kind::Item, index}; }
   static FocusTarget reset() { return {Kind::Reset, 0}; }
   static FocusTarget back() { return {Kind::Back, 0}; }
   bool operator==(const FocusTarget& other) const
   { return kind == other.kind && index == other.index; }
};

/* The one place for keyboard and pointer focus. We keep the cursor of each
 * region while another screen is open, wrap every linear walk the same way,
 * add the slot-grid rule for Pause in navigation, and read it in the views. */
class Focus
{
public:
   const FocusTarget& target(FocusRegion region) const
   { return targets[static_cast<size_t>(region)]; }
   void set(FocusRegion region, FocusTarget target)
   { targets[static_cast<size_t>(region)] = target; }
   int position(FocusRegion region) const { return target(region).index; }
   void position(FocusRegion region, int index) { set(region, FocusTarget::item(index)); }
   static int ring(int position, int count, int direction)
   { return count > 0 ? ((position + direction) % count + count) % count : 0; }
   int move(FocusRegion region, int count, int direction)
   {
      const int next = ring(position(region), count, direction);
      position(region, next);
      return next;
   }
   FocusTarget next(FocusRegion region, const std::vector<FocusTarget>& stops,
         int direction) const
   {
      for (size_t index = 0; index < stops.size(); ++index)
         if (stops[index] == target(region))
            return stops[ring((int)index, (int)stops.size(), direction)];
      return stops.empty() ? FocusTarget{} : stops.front();
   }

   int pause_row() const
   { return target(FocusRegion::Pause).kind == FocusTarget::Kind::Slots
         ? -1 : position(FocusRegion::Pause); }
   void pause_row(int index)
   { set(FocusRegion::Pause, index < 0
         ? FocusTarget{FocusTarget::Kind::Slots, 0} : FocusTarget::item(index)); }
   const Event& pause_action() const { return pause_intent; }
   void pause_action(Event action)
   {
      pause_intent = std::move(action);
      pause_id.clear();
   }
   const std::string& pause_element() const { return pause_id; }
   void pause_element(const char *id) { pause_id = id ? id : ""; }
   int highlighted_slot() const
   {
      return pause_id.empty() && pause_intent.kind == RIB_RMLUI_ACTION_SELECT_SLOT
            ? pause_intent.slot : 0;
   }

private:
   std::array<FocusTarget, 4> targets{};
   /* When the focus goes back to a slot, we keep the last action for the
    * move-cue rule. We paint the row by the element id. */
   Event pause_intent{RIB_RMLUI_ACTION_RESUME};
   std::string pause_id = document_contract::Resume;
};
}
