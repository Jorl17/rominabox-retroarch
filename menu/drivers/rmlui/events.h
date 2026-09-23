#ifndef RIB_MENU_EVENTS_H
#define RIB_MENU_EVENTS_H

enum rib_rmlui_action
{
   RIB_RMLUI_ACTION_NONE = 0,
   RIB_RMLUI_ACTION_RESUME,
   RIB_RMLUI_ACTION_SAVE,
   RIB_RMLUI_ACTION_LOAD,
   RIB_RMLUI_ACTION_CONTROLS,
   RIB_RMLUI_ACTION_QUIT,
   RIB_RMLUI_ACTION_SELECT_SLOT_1,
   RIB_RMLUI_ACTION_SELECT_SLOT_2,
   RIB_RMLUI_ACTION_SELECT_SLOT_3,
   RIB_RMLUI_ACTION_SELECT_SLOT_4,
   RIB_RMLUI_ACTION_SELECT_SLOT_5,
   RIB_RMLUI_ACTION_SELECT_SLOT_6,
   RIB_RMLUI_ACTION_CONTROLS_BACK,
   RIB_RMLUI_ACTION_CONTROLS_RESET,
   RIB_RMLUI_ACTION_CONTROLS_CANCEL,
   RIB_RMLUI_ACTION_CONTROL_FIRST,
   RIB_RMLUI_ACTION_CONTROL_LAST = RIB_RMLUI_ACTION_CONTROL_FIRST + 47,

   RIB_RMLUI_ACTION_DEVICE_PICKER_TOGGLE,
   RIB_RMLUI_ACTION_DEVICE_PICKER_CHOOSE,
   RIB_RMLUI_ACTION_SLIDER,
   RIB_RMLUI_ACTION_TOGGLE,
   RIB_RMLUI_ACTION_LIST_CHOOSE,
   RIB_RMLUI_ACTION_LIST_PAGE,
   RIB_RMLUI_ACTION_PART_TOGGLE,
   RIB_RMLUI_ACTION_SHOW_SCREEN
};

#ifdef __cplusplus
#include <array>
#include <string>
#include <utility>

namespace rib {
/* Each queued intent has a copy of its payload, so later clicks cannot
 * overwrite the id or value of an intent we have not handled yet. */
struct Event
{
   rib_rmlui_action kind;
   std::string id;
   float fraction;
   bool on;
   Event(rib_rmlui_action kind = RIB_RMLUI_ACTION_NONE, std::string id = {},
         float fraction = 0.0f, bool on = false)
      : kind(kind), id(std::move(id)), fraction(fraction), on(on) {}
};

class EventQueue
{
public:
   void push(Event event)
   {
      if (event.kind == RIB_RMLUI_ACTION_NONE || count == items.size()) return;
      items[(head + count++) % items.size()] = std::move(event);
   }
   Event take()
   {
      if (!count) return {};
      Event event = std::move(items[head]);
      head = (head + 1) % items.size();
      --count;
      return event;
   }
   void clear() { head = count = 0; }
private:
   std::array<Event, 8> items;
   size_t head = 0, count = 0;
};
}
rib::Event rib_rmlui_take_event();
#endif
#endif
