#pragma once
#include "events.h"
#include "menu_api.h"
#include "sounds.hpp"
#include <RmlUi/Core.h>
#include <functional>
#include <map>
#include <string>

namespace rib {
class Focus;
class Screens;
class Controls;
class Slots;
class Document;
class Lists;
class Parts;

/* Move the focus for an arrow with the RmlUi navigation, from the focused
 * element to the next stop on screen, or not at all at an edge, and play the
 * move cue for a move. Use this for every arrow, including the physical keys
 * of the text path. Returns whether the focus moved. */
inline bool navigate(Rml::Context *context, rib_key key)
{
   Rml::Input::KeyIdentifier identifier;
   switch (key)
   {
      case RIB_KEY_UP: identifier = Rml::Input::KI_UP; break;
      case RIB_KEY_DOWN: identifier = Rml::Input::KI_DOWN; break;
      case RIB_KEY_LEFT: identifier = Rml::Input::KI_LEFT; break;
      case RIB_KEY_RIGHT: identifier = Rml::Input::KI_RIGHT; break;
      default: return false;
   }
   if (!context)
      return false;
   /* We search by layout boxes, and a panel shown in this frame has none yet. */
   context->Update();
   Rml::Element *before = context->GetFocusElement();
   context->ProcessKeyDown(identifier, 0);
   context->ProcessKeyUp(identifier, 0);
   if (context->GetFocusElement() == before)
      return false;
   play_move_sound(key == RIB_KEY_UP || key == RIB_KEY_LEFT);
   return true;
}

/* Keys, screens and focus. We let RmlUi choose where an arrow goes, and keep
 * here the rules of our menu. Left and Right move sliders and turn list
 * pages. The arrows stay inside an open picker or dialog. We start a screen
 * on its first stop, and on return we focus the element the player left it
 * from. We return immediate intents to Menu to dispatch, and on OK we click
 * the focused element, through the same listener as for a pointer.
 *
 * We show every screen through here, and finish each one the same way, on
 * screen, focused, and reported to `shown`. */
class Navigation
{
public:
   Navigation(Focus& focus, Screens& screens, Controls& controls, Slots& slots,
         Document& document, Lists& lists, Parts& parts)
      : focus(focus), screens(screens), controls(controls), slots(slots),
        document(document), lists(lists), parts(parts) {}
   /* What we do in the menu after we show any screen. */
   void on_shown(std::function<void()> shown) { this->shown = std::move(shown); }
   Event key(rib_key action);
   /* We open the menu on Pause, at the start marked in the design. */
   void open();
   /* On close, we take Pause as showing, because we reopen the menu there. */
   void close();
   /* Show a screen and focus it. Showing the screen the player came from is a
    * return, so we focus the element the player left it from and play the
    * cancel cue. Otherwise we go forward and play the confirm cue. */
   bool show(const std::string& id);
   /* The screen the player opened the current one from, or Pause. */
   void back();
   /* Focus the first stop of the open dialog, or else the first stop of the
    * current screen, or the remembered element. */
   void enter();
   /* Turn the list with `from` (or the visible one) and focus its first row. */
   bool turn_page(int delta, Rml::Element *from = nullptr);
   /* While a dialog is open, the arrows stay in it, and when it closes we put
    * the focus back where it was. nullptr when no dialog is open. */
   void hold(Rml::Element *dialog);
private:
   /* Show `id` and focus it. We show screens only through this. */
   bool present(const std::string& id);
   Event move(rib_key action);
   Event back_key();
   Rml::Element *panel() const;
   Rml::Element *element(const std::string& id) const;
   Focus& focus;
   Screens& screens;
   Controls& controls;
   Slots& slots;
   Document& document;
   Lists& lists;
   Parts& parts;
   std::function<void()> shown;
   /* The screen from which the player opened each screen, for Back. */
   std::map<std::string, std::string> openers;
   std::string before_dialog;
   bool dialog_held = false;
};
}
