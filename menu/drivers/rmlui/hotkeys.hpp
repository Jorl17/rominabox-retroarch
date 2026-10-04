#pragma once

#include "capture_pointer.hpp"
#include "events.h"
#include "hotkey_bindings.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace Rml { class Element; }

namespace rib {
class Controls;
class Document;
class Focus;
class Screens;
class Status;

/* The HOTKEYS screen, with a row of chips for the bindings of each hotkey.
 * The player adds one with +, through the same capture as Controls, removes
 * one by choosing its chip, and restores the defaults with RESET. Here we
 * show the bindings from HotkeyBindings, and the game inputs from `controls`. */
class Hotkeys
{
public:
   Hotkeys(Document& document, Focus& focus, Screens& screens, Status& status,
         EventQueue& intents, Event& hovered, CapturePointer& pointer, const Controls& controls)
      : document(document), focus(focus), screens(screens), status(status),
        intents(intents), hovered(hovered), pointer(pointer), controls(controls) {}
   /* The bindings, read before any document, because we need them while the
    * game runs, before we have ever drawn the menu. */
   void load(const char *assets, const char *data) { bindings.load(assets, data); }
   const HotkeyBindings& read() const { return bindings; }
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
      Hotkey hotkey = Hotkey::Menu;
      /* `mode` is empty for a hotkey with only one way of working. */
      std::string add, label, mode;
      std::vector<std::string> chips;
   };
   /* The design's word for the current way of `hotkey`. */
   std::string mode_word(Hotkey hotkey) const;
   const Row *row(Hotkey hotkey) const;
   std::string name(Hotkey hotkey) const;
   void start_capture(Hotkey hotkey);
   void end_capture(const std::string& words);
   void remove(Hotkey hotkey, int chip);
   void reset();
   /* The words that describe the result of a change. */
   std::string outcome(const HotkeyBindings::Change& change) const;
   Document& document;
   Focus& focus;
   Screens& screens;
   Status& status;
   EventQueue& intents;
   Event& hovered;
   CapturePointer& pointer;
   const Controls& controls;
   HotkeyBindings bindings;
   std::vector<Row> rows;
   struct Capture
   {
      bool active = false;
      Hotkey hotkey = Hotkey::Menu;
   } capture;
};
}
