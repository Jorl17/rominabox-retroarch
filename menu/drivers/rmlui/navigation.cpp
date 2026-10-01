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

namespace rib {
Rml::Element *Navigation::element(const std::string& id) const
{
   return document.root() && !id.empty() ? document.root()->GetElementById(id) : nullptr;
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
   if (focus.set(focus.recall(screens.current())))
      return;
   /* On every screen, we start where the design marks the start. When the pad
    * screen has no mark, we start on its first control, not on the picker
    * that comes before the controls in the document. */
   if (focus.set(focus.marked(panel())))
      return;
   if (screens.showing(ScreenRole::Controls))
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

bool Navigation::present(const std::string& id)
{
   /* A copy, because we may be about to forget `id` as an opener. */
   const std::string to = id;
   if (!screens.show_screen(to))
      return false;
   screens.remember(to);
   /* Split its lists again from the visible rows, because a row we showed or
    * hid since the last split, such as a switch with no effect in the game,
    * would leave a gap on a page, or an empty page. */
   lists.resplit(panel(), focus.remembered(to));
   enter();
   if (shown)
      shown();
   return true;
}

void Navigation::open()
{
   openers.clear();
   focus.forget();
   focus.trap(nullptr);
   dialog_held = false;
   before_dialog.clear();
   controls.close_picker();
   close();
   present(screens.current());
}

void Navigation::close()
{
   screens.remember(screens.with_role(ScreenRole::Pause));
}

bool Navigation::show(const std::string& id)
{
   const std::string from = screens.current();
   const std::string to = id;
   if (screens.screen_panel(to).empty())
      return false;
   const auto opener = openers.find(from);
   const bool returning = opener != openers.end() && opener->second == to;
   if (from != to)
   {
      controls.close_picker();
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
   if (!present(to))
      return false;
   play_action_sound(returning ? RIB_RMLUI_ACTION_CONTROLS_BACK : RIB_RMLUI_ACTION_SHOW_SCREEN);
   return true;
}

void Navigation::back()
{
   const auto opener = openers.find(screens.current());
   show(opener != openers.end() ? opener->second : screens.with_role(ScreenRole::Pause));
}

bool Navigation::turn_page(int delta, Rml::Element *from)
{
   Rml::Element *list = Lists::list_of(from);
   if (lists.turn_list_page(delta, list) < 0)
      return false;
   /* When the player turns the page with an arrow, we keep the focus on it,
    * so another press turns again. At the end we move the focus to the other
    * arrow. After a turn from a row, we focus the first stop of the page. */
   Rml::Element *to = focus.first(lists.shown_page(list));
   const bool previous = from && from->IsClassSet(document_contract::ListPagerPrev);
   if (previous || (from && from->IsClassSet(document_contract::ListPagerNext)))
   {
      Rml::Element *other = find_class(list, previous
            ? document_contract::ListPagerNext : document_contract::ListPagerPrev);
      if (!from->IsClassSet(document_contract::Disabled))
         to = from;
      else if (other && !other->IsClassSet(document_contract::Disabled))
         to = other;
   }
   focus.set(to);
   return true;
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
      controls.focus_picker();
   /* Closing the dialog may already have moved the focus somewhere useful. */
   if (focus.stop(focus.current()))
      return;
   if (!focus.set(before_dialog.c_str()))
      enter();
}

Event Navigation::back_key()
{
   if (controls.device_picker_open)
   {
      controls.close_picker();
      controls.focus_picker();
      play_action_sound(RIB_RMLUI_ACTION_CONTROLS_CANCEL);
      return {};
   }
   if (screens.showing(ScreenRole::Pause))
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
   /* On a list page, Left and Right move to the next stop on the same page,
    * when there is one, and turn the page at its edge. */
   if (Rml::Element *page = sideways ? Lists::page_of(from) : nullptr)
   {
      const bool beside = step(document.get_context(), action)
            && Lists::page_of(focus.current()) == page;
      if (!beside)
         focus.set(from);
      if (beside || turn_page(direction, from))
         play_move_sound(action == RIB_KEY_LEFT);
      focus.paint();
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
   if (capturing && capturing())
   {
      if (action == RIB_KEY_CANCEL || action == RIB_KEY_RESUME ||
          action == RIB_KEY_TOGGLE)
         return screens.showing(ScreenRole::Hotkeys)
               ? RIB_RMLUI_ACTION_HOTKEYS_CANCEL : RIB_RMLUI_ACTION_CONTROLS_CANCEL;
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
         return back_key();
      case RIB_KEY_START:
         if (screens.showing(ScreenRole::Pause))
            return RIB_RMLUI_ACTION_SAVE;
         if (screens.showing(ScreenRole::Controls))
            return RIB_RMLUI_ACTION_CONTROLS_RESET;
         return {};
      default:
         return {};
   }
}
}
