#ifndef RIB_MENU_EVENTS_H
#define RIB_MENU_EVENTS_H

enum rib_rmlui_action
{
   RIB_RMLUI_ACTION_NONE = 0,
   RIB_RMLUI_ACTION_RESUME,
   RIB_RMLUI_ACTION_SAVE,
   RIB_RMLUI_ACTION_LOAD,
   RIB_RMLUI_ACTION_QUIT,
   RIB_RMLUI_ACTION_SELECT_SLOT,
   RIB_RMLUI_ACTION_CONTROLS_BACK,
   RIB_RMLUI_ACTION_CONTROLS_RESET,
   RIB_RMLUI_ACTION_CONTROLS_CANCEL,
   RIB_RMLUI_ACTION_CONTROL,

   RIB_RMLUI_ACTION_DEVICE_PICKER_TOGGLE,
   RIB_RMLUI_ACTION_DEVICE_PICKER_CHOOSE,
   RIB_RMLUI_ACTION_SLIDER,
   RIB_RMLUI_ACTION_LIST_CHOOSE,
   RIB_RMLUI_ACTION_LIST_PAGE,
   RIB_RMLUI_ACTION_PART_TOGGLE,
   RIB_RMLUI_ACTION_SHOW_SCREEN,
   RIB_RMLUI_ACTION_ACCOUNT,
   /* A button on a list screen, passed to the list for that screen. */
   RIB_RMLUI_ACTION_LIST_ACTION,
   /* HOTKEYS: capture a binding for the hotkey `id`, remove its binding in
    * chip `slot`, switch the way it works, restore the game's own bindings,
    * or end a capture. */
   RIB_RMLUI_ACTION_HOTKEY_ADD,
   RIB_RMLUI_ACTION_HOTKEY_REMOVE,
   RIB_RMLUI_ACTION_HOTKEY_MODE,
   RIB_RMLUI_ACTION_HOTKEYS_RESET,
   RIB_RMLUI_ACTION_HOTKEYS_CANCEL,
   /* UNINSTALL or RESET, confirmed: we forget the game and close it. */
   RIB_RMLUI_ACTION_FORGET
};

#ifdef __cplusplus
#include <array>
#include <string>
#include <utility>

namespace rib {
enum class AccountAction { Open, SignIn, Cancel, RevealPassword, Enable, SignOut, Retry, KeepSession, EndSession, SkipStartup };
/* Each queued intent has a copy of its payload, so later clicks cannot
 * overwrite the id or value of an intent we have not handled yet. */
struct Event
{
   rib_rmlui_action kind;
   std::string id;
   float fraction;
   bool on;
   int slot = 0;
   /* A page turned: -1 back, 1 on. */
   int direction = 0;
   AccountAction account = AccountAction::Open;
   Event(rib_rmlui_action kind = RIB_RMLUI_ACTION_NONE, std::string id = {},
         float fraction = 0.0f, bool on = false)
      : kind(kind), id(std::move(id)), fraction(fraction), on(on) {}
   static Event account_action(AccountAction action)
   {
      Event event(RIB_RMLUI_ACTION_ACCOUNT);
      event.account = action;
      return event;
   }
   static Event hotkey(rib_rmlui_action kind, std::string action, int chip = 0)
   {
      Event event(kind, std::move(action));
      event.slot = chip;
      return event;
   }
   static Event select_slot(int number)
   {
      Event event(RIB_RMLUI_ACTION_SELECT_SLOT);
      event.slot = number;
      return event;
   }
   bool same_target(const Event& other) const
   {
      return kind == other.kind && id == other.id && slot == other.slot
            && direction == other.direction && account == other.account;
   }
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
#endif
#endif
