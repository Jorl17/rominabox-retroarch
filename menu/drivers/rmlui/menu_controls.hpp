#pragma once

#include "capture_pointer.hpp"
#include "events.h"
#include "menu_bindings.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace Rml { class Element; }

namespace rib {
class Document;
class Focus;
class Screens;
class Status;

/* The MENU CONTROLS screen, with a row of chips for the bindings of each
 * action of the menu. The player adds one with +, through the same capture
 * as Controls, removes one by choosing its chip, and restores the defaults
 * with RESET. Here we show the bindings from MenuBindings and the result of
 * each change. */
class MenuControls
{
public:
   MenuControls(Document& document, Focus& focus, Screens& screens, Status& status,
         EventQueue& intents, Event& hovered, CapturePointer& pointer)
      : document(document), focus(focus), screens(screens), status(status),
        intents(intents), hovered(hovered), pointer(pointer) {}
   /* The bindings, read before any document, because we need them while the
    * game runs, before we have ever drawn the menu. */
   void load(const char *assets, const char *data) { bindings.load(assets, data); }
   const MenuBindings& read() const { return bindings; }
   /* Once per document, find its rows and the number of chips in each, and
    * add a listener to each chip and to +. */
   void bind();
   /* Handle an event for this screen, or return false when it is for another.
    * While a capture is waiting, we handle only its CANCEL. */
   bool handle(const Event& event);
   void screen_shown(bool showing);
   void poll_capture();
   void cancel_capture();
   bool capturing() const { return capture.active; }
   void refresh();

private:
   struct Row
   {
      MenuAction action = MenuAction::Menu;
      std::string add, label;
      std::vector<std::string> chips;
   };
   const Row *row(MenuAction action) const;
   std::string name(MenuAction action) const;
   void start_capture(MenuAction action);
   void end_capture(const std::string& words);
   void remove(MenuAction action, int chip);
   void reset();
   /* The words that describe the result of a change. */
   std::string outcome(const MenuBindings::Change& change) const;
   Document& document;
   Focus& focus;
   Screens& screens;
   Status& status;
   EventQueue& intents;
   Event& hovered;
   CapturePointer& pointer;
   MenuBindings bindings;
   std::vector<Row> rows;
   struct Capture
   {
      bool active = false;
      MenuAction action = MenuAction::Menu;
   } capture;
};
}
