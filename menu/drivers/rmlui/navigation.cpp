#include "navigation.hpp"
#include "focus.hpp"
#include "screens.hpp"
#include "controls.hpp"
#include "slots.hpp"
#include "sounds.hpp"
#include "document.hpp"
#include "elements.hpp"
#include "lists.hpp"
#include "parts.hpp"
#include <cstdlib>
#include <cstring>
#include <string/stdstring.h>

namespace rib {
Rml::Element *Navigation::element(const char *id) const
{
   return document.root() && id && *id ? document.root()->GetElementById(id) : nullptr;
}

Rml::Element *Navigation::panel() const
{
   return element(screens.screen_panel(screens.current()));
}

void Navigation::enter()
{
   /* We start in an open dialog, because the panel behind it is inactive. */
   if (dialog_held && focus.set(focus.first(focus.trapped())))
      return;
   const char *screen = screens.current();
   if (focus.set(focus.recall(screen)))
      return;
   /* We start the pad screen on its first control, not on the picker that
    * comes before the controls in the document. */
   if (screens.controls_visible())
   {
      controls.focus(controls.first());
      if (focus.stop(focus.current()))
         return;
   }
   if (focus.set(focus.first(panel())))
      return;
   /* When there is nothing to focus, we highlight nothing, and not an element
    * of the screen that we hid. */
   if (document.root())
      document.root()->Focus();
   focus.paint();
}

void Navigation::open()
{
   openers.clear();
   focus.forget();
   focus.trap(nullptr);
   dialog_held = false;
   before_dialog.clear();
   if (controls.device_picker_open)
      controls.toggle_picker();
   screens.show_screen("pause");
   screens.remember("pause");
   if (!focus.set(document_contract::Resume))
      enter();
}

bool Navigation::show(const char *id)
{
   /* A copy, because we may forget `id` as an opener here. */
   const std::string from = screens.current();
   const std::string to = id ? id : "";
   if (!screens.show_screen(to.c_str()))
      return false;
   const auto opener = openers.find(from);
   const bool returning = opener != openers.end() && opener->second == to;
   if (from != to)
   {
      if (controls.device_picker_open)
         controls.toggle_picker();
      if (!dialog_held)
         focus.trap(nullptr);
      if (returning)
      {
         openers.erase(opener);
         focus.forget(from);
      }
      else
      {
         focus.remember(from);
         openers[to] = from;
      }
   }
   screens.remember(to.c_str());
   enter();
   play_action_sound(returning ? RIB_RMLUI_ACTION_CONTROLS_BACK : RIB_RMLUI_ACTION_SHOW_SCREEN);
   return true;
}

void Navigation::back_to_opener()
{
   const auto opener = openers.find(screens.current());
   show(opener != openers.end() ? opener->second.c_str() : "pause");
}

void Navigation::select_slot(int slot)
{
   if (!valid_slot(slot))
      return;
   slots.set_selected_slot(slot);
   focus.set((document_contract::Slot + std::to_string(slot)).c_str());
}

bool Navigation::turn_page(int delta, Rml::Element *from)
{
   Rml::Element *list = from;
   while (list && !list->IsClassSet(document_contract::List))
      list = list->GetParentNode();
   if (lists.turn_list_page(delta, list) < 0)
      return false;
   focus.set(lists.first_row(list));
   return true;
}

void Navigation::picker()
{
   Rml::Element *box = element(document_contract::ControlsDevice);
   if (controls.device_picker_open && box)
   {
      focus.trap(box);
      Rml::Element *chosen = nullptr;
      walk(box, [&](Rml::Element *option) {
         if (option->IsClassSet(document_contract::Selected)
               && option->GetId().rfind(document_contract::ControlsDeviceOptionPrefix, 0) == 0)
         {
            chosen = option;
            return Walk::Stop;
         }
         return Walk::Continue;
      });
      if (!focus.set(chosen))
         focus.set(focus.first(element(document_contract::ControlsDeviceList)));
      return;
   }
   if (!dialog_held)
      focus.trap(nullptr);
   focus.set(document_contract::ControlsDeviceCurrent);
}

void Navigation::hold(Rml::Element *dialog)
{
   if (dialog)
   {
      if (dialog_held && focus.trapped() == dialog)
         return;
      if (!dialog_held)
         before_dialog = focus.current_id();
      dialog_held = true;
      focus.trap(dialog);
      Rml::Element *current = focus.current();
      bool inside = false;
      for (Rml::Element *at = current; at; at = at->GetParentNode())
         inside = inside || at == dialog;
      if (!inside || !focus.stop(current))
         focus.set(focus.first(dialog));
      return;
   }
   if (!dialog_held)
      return;
   dialog_held = false;
   focus.trap(nullptr);
   if (controls.device_picker_open)
      picker();
   /* Closing the dialog may already have moved the focus somewhere useful. */
   if (focus.stop(focus.current()))
      return;
   if (!focus.set(before_dialog.c_str()))
      enter();
}

Event Navigation::back()
{
   if (controls.device_picker_open)
   {
      controls.toggle_picker();
      picker();
      play_action_sound(RIB_RMLUI_ACTION_CONTROLS_CANCEL);
      return {};
   }
   if (string_is_equal(screens.current(), "pause"))
      return RIB_RMLUI_ACTION_RESUME;
   /* The back button of the screen, with its declared destination, if any. */
   Rml::Element *own = nullptr;
   walk(panel(), [&](Rml::Element *element) {
      if (display_none(element))
         return Walk::SkipChildren;
      if (element->IsClassSet(document_contract::ListBack)
            || element->IsClassSet(document_contract::OptionsBack))
      {
         own = element;
         return Walk::Stop;
      }
      return Walk::Continue;
   });
   if (own)
   {
      own->Click();
      return {};
   }
   return RIB_RMLUI_ACTION_CONTROLS_BACK;
}

Event Navigation::move(rib_key action)
{
   Rml::Element *from = focus.current();
   if (!focus.stop(from))
   {
      /* When the focus is on something gone, such as a page turned away or
       * a disabled button, the first key press only shows the highlight. */
      enter();
      return {};
   }
   const bool sideways = action == RIB_KEY_LEFT || action == RIB_KEY_RIGHT;
   const int direction = action == RIB_KEY_RIGHT || action == RIB_KEY_DOWN ? 1 : -1;
   if (sideways && from->IsClassSet(document_contract::Slider))
   {
      parts.nudge_slider(from->GetId().c_str(), direction);
      return {};
   }
   if (sideways && from->IsClassSet(document_contract::ListRow))
   {
      if (turn_page(direction, from))
         play_move_sound(action == RIB_KEY_LEFT);
      return {};
   }
   if (navigate(document.get_context(), action))
   {
      Rml::Element *to = focus.current();
      /* With SAVE and LOAD we use the slot the player reached with a key.
       * Moving the pointer over a slot does not select it. */
      if (to && to->IsClassSet(document_contract::SlotClass)
            && to->GetId().rfind(document_contract::Slot, 0) == 0)
         slots.set_selected_slot(std::atoi(to->GetId().c_str() + std::strlen(document_contract::Slot)));
   }
   focus.paint();
   return {};
}

Event Navigation::key(rib_key action)
{
   if (controls.capture_active)
   {
      if (action == RIB_KEY_CANCEL || action == RIB_KEY_RESUME ||
          action == RIB_KEY_TOGGLE)
         return RIB_RMLUI_ACTION_CONTROLS_CANCEL;
      return {};
   }

   switch (action)
   {
      case RIB_KEY_UP:
      case RIB_KEY_DOWN:
      case RIB_KEY_LEFT:
      case RIB_KEY_RIGHT:
         return move(action);
      case RIB_KEY_OK:
      case RIB_KEY_SELECT:
      {
         /* We go through the listener of the element, as for a pointer. */
         Rml::Element *focused = focus.current();
         if (focus.stop(focused))
            focused->Click();
         else
            enter();
         return {};
      }
      case RIB_KEY_CANCEL:
      case RIB_KEY_RESUME:
      case RIB_KEY_TOGGLE:
         return back();
      case RIB_KEY_START:
         if (string_is_equal(screens.current(), "pause"))
            return RIB_RMLUI_ACTION_SAVE;
         if (screens.controls_visible())
            return RIB_RMLUI_ACTION_CONTROLS_RESET;
         return {};
      default:
         return {};
   }
}
}
