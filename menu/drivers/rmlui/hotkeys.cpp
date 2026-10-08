#include "hotkeys.hpp"

#include "controls.hpp"
#include "document.hpp"
#include "document_contract.hpp"
#include "focus.hpp"
#include "host.h"
#include "listeners.hpp"
#include "screens.hpp"
#include "sounds.hpp"
#include "status.hpp"
#include "words.hpp"
#include <cctype>
#include <string>

namespace rib {
namespace {
std::string row_id(Hotkey hotkey)
{
   return std::string(document_contract::HotkeyPrefix) + hotkey_id(hotkey);
}

void listen(Rml::Element *element, EventQueue& intents, Event& hovered, const Event& action)
{
   if (!element)
      return;
   element->AddEventListener(Rml::EventId::Click, new ActionListener(intents, action));
   element->AddEventListener(Rml::EventId::Mouseover, new HoverListener(hovered, action));
   element->AddEventListener(Rml::EventId::Mouseout, new HoverListener(hovered, action));
}
}

void Hotkeys::bind()
{
   rows.clear();
   Rml::Element *root = document.root();
   if (!root)
      return;
   const Hotkey *hotkeys = all_hotkeys();
   for (size_t index = 0; index < kHotkeyCount; ++index)
   {
      Row row;
      row.hotkey = hotkeys[index];
      const std::string base = row_id(row.hotkey);
      row.add = base + document_contract::AddSuffix;
      row.label = base + document_contract::LabelSuffix;
      Rml::Element *add = root->GetElementById(row.add);
      /* When the game has no HOTKEYS screen, there are no rows. */
      if (!add)
         continue;
      listen(add, intents, hovered,
            Event::hotkey(RIB_RMLUI_ACTION_HOTKEY_ADD, hotkey_id(row.hotkey)));
      for (int chip = 1;; ++chip)
      {
         const std::string id = base + "-" + std::to_string(chip);
         Rml::Element *element = root->GetElementById(id);
         if (!element)
            break;
         listen(element, intents, hovered, Event::hotkey(
               RIB_RMLUI_ACTION_HOTKEY_REMOVE, hotkey_id(row.hotkey), chip));
         row.chips.push_back(id);
      }
      const std::string mode = base + document_contract::ModeSuffix;
      Rml::Element *way = bindings.modes(row.hotkey).empty() ? nullptr : root->GetElementById(mode);
      if (way)
      {
         listen(way, intents, hovered,
               Event::hotkey(RIB_RMLUI_ACTION_HOTKEY_MODE, hotkey_id(row.hotkey)));
         row.mode = mode;
      }
      rows.push_back(row);
   }
   if (rows.empty())
      return;
   listen(root->GetElementById(document_contract::HotkeysReset), intents, hovered,
         Event(RIB_RMLUI_ACTION_HOTKEYS_RESET));
   listen(root->GetElementById(document_contract::HotkeysCancel), intents, hovered,
         Event(RIB_RMLUI_ACTION_HOTKEYS_CANCEL));
   if (Rml::Element *back = root->GetElementById(document_contract::HotkeysBack))
      back->AddEventListener(Rml::EventId::Click, new ReturnListener(intents));
   refresh();
}

const Hotkeys::Row *Hotkeys::row(Hotkey hotkey) const
{
   for (const Row& each : rows)
      if (each.hotkey == hotkey)
         return &each;
   return nullptr;
}

/* The name of the hotkey on its row, which is the word from the design. When
 * the design has no label, the id in capitals with spaces between words. */
std::string Hotkeys::name(Hotkey hotkey) const
{
   const Row *shown = row(hotkey);
   Rml::Element *label = shown && document.root()
         ? document.root()->GetElementById(shown->label) : nullptr;
   std::string text = label ? label->GetInnerRML() : std::string();
   if (text.empty())
      for (const char *at = hotkey_id(hotkey); *at; ++at)
         text += *at == '-' ? ' ' : (char)std::toupper((unsigned char)*at);
   return text;
}

std::string Hotkeys::mode_word(Hotkey hotkey) const
{
   const std::vector<std::string> modes = bindings.modes(hotkey);
   if (modes.empty())
      return std::string();
   const std::string& mode = modes[bindings.mode(hotkey)];
   Word word;
   return word_named(std::string("hotkey-mode-") + mode, word) ? say(word) : mode;
}

void Hotkeys::refresh()
{
   for (const Row& shown : rows)
   {
      const std::vector<HotkeyBinding>& list = bindings.of(shown.hotkey);
      /* A hotkey that the game does not have: we disable its row and every
       * part of it, and the design sets whether that is visible. */
      const bool offered = bindings.offered(shown.hotkey);
      document.set_disabled(row_id(shown.hotkey).c_str(), !offered);
      if (!shown.mode.empty())
      {
         document.set_element_text(shown.mode.c_str(), mode_word(shown.hotkey).c_str());
         document.set_disabled(shown.mode.c_str(), !offered);
      }
      for (size_t chip = 0; chip < shown.chips.size(); ++chip)
      {
         const char *id = shown.chips[chip].c_str();
         const bool used = chip < list.size();
         document.set_shown(id, used);
         if (!used)
            continue;
         document.set_element_text(id, bindings.words(list[chip]).c_str(),
               document_contract::HotkeyWords);
         document.set_class(id, document_contract::ChipKey,
               list[chip].kind == HotkeyBinding::Kind::Key);
         document.set_class(id, document_contract::ChipPad,
               list[chip].kind == HotkeyBinding::Kind::Pad);
      }
      document.set_disabled(shown.add.c_str(), !offered || list.size() >= shown.chips.size());
      document.set_class(shown.add.c_str(), document_contract::Capturing,
            capture.active && capture.hotkey == shown.hotkey);
   }
   if (rows.empty())
      return;
   document.set_class(document_contract::HotkeysCancel, document_contract::Capturing,
         capture.active);
   document.set_shown(document_contract::HotkeysCancel, capture.active);
}

std::string Hotkeys::outcome(const HotkeyBindings::Change& change) const
{
   using Outcome = HotkeyBindings::Outcome;
   const std::string other = name(change.other);
   switch (change.outcome)
   {
      case Outcome::Unchanged: return say(Word::BindingUnchanged);
      case Outcome::Full: return say(Word::BindingNoRoom, {{"control", other}});
      case Outcome::NeedsBinding: return say(Word::BindingNeeded, {{"control", other}});
      case Outcome::NeedsKey: return say(Word::KeyNeeded, {{"control", other}});
      case Outcome::GameInput: return say(Word::GameInput, {{"control", change.control}});
      default: break;
   }
   if (!change.saved)
      return say(Word::BindingSaveFailed);
   switch (change.outcome)
   {
      case Outcome::Swapped: return say(Word::BindingSwapped, {{"control", other}});
      case Outcome::Moved: return say(Word::BindingMoved, {{"control", other}});
      case Outcome::Removed: return say(Word::BindingRemoved);
      default: return say(Word::BindingSaved);
   }
}

void Hotkeys::start_capture(Hotkey hotkey)
{
   const Row *shown = row(hotkey);
   if (!shown)
      return;
   if (!rib_host_capture_input_start(RIB_CONTROL_CAPTURE_SECONDS))
   {
      status.set_hotkeys(say(Word::CaptureFailed).c_str());
      return;
   }
   capture.active = true;
   capture.hotkey = hotkey;
   pointer.start();
   focus.set(shown->add.c_str());
   status.set_hotkeys(say(Word::CaptureCountdown, {{"control", name(hotkey)},
         {"seconds", std::to_string(RIB_CONTROL_CAPTURE_SECONDS)}}).c_str());
   screens.set_footer_hint(say(Word::CancelHint).c_str());
   refresh();
}

void Hotkeys::end_capture(const std::string& words)
{
   capture.active = false;
   status.set_hotkeys(words.c_str());
   screens.restore_footer();
   refresh();
   if (const Row *shown = row(capture.hotkey))
      focus.set(shown->add.c_str());
}

void Hotkeys::cancel_capture()
{
   if (!capture.active)
      return;
   rib_host_capture_cancel();
   end_capture(say(Word::BindingUnchanged));
}

void Hotkeys::poll_capture()
{
   if (!capture.active)
      return;
   float remaining = 0.0f;
   switch (rib_host_capture_poll(pointer.counts(), &remaining))
   {
      case RIB_CAPTURE_CAPTURED:
      {
         char text[128];
         rib_host_captured_input(text, sizeof(text));
         HotkeyBinding binding;
         const bool read = *text && read_hotkey_binding(text, binding);
         if (!read)
            end_capture(say(Word::BindingUnusable));
         else if (cancels_capture(binding))
            end_capture(say(Word::BindingUnchanged));
         else
         {
            std::vector<size_t> room;
            const Hotkey *hotkeys = all_hotkeys();
            for (size_t index = 0; index < kHotkeyCount; ++index)
            {
               const Row *shown = row(hotkeys[index]);
               room.push_back(shown ? shown->chips.size() : 0);
            }
            end_capture(outcome(bindings.add(capture.hotkey, binding, room,
                  controls.game_inputs())));
         }
         break;
      }
      case RIB_CAPTURE_TIMED_OUT:
         end_capture(say(Word::CaptureTimeout));
         break;
      default:
         status.set_hotkeys(say(Word::CaptureCountdown, {{"control", name(capture.hotkey)},
               {"seconds", std::to_string((unsigned)(remaining + 0.999f))}}).c_str());
         break;
   }
}

void Hotkeys::remove(Hotkey hotkey, int chip)
{
   const Row *shown = row(hotkey);
   if (!shown || chip < 1)
      return;
   status.set_hotkeys(outcome(bindings.remove(hotkey, (size_t)(chip - 1))).c_str());
   refresh();
   /* We keep the focus where it was, or move it to the next binding, to the
    * last one left, or to + when none is left. */
   const size_t left = bindings.of(hotkey).size();
   if (left == 0)
      focus.set(shown->add.c_str());
   else if ((size_t)chip > left)
      focus.set(shown->chips[left - 1].c_str());
}

void Hotkeys::reset()
{
   cancel_capture();
   const bool removed = bindings.reset();
   status.set_hotkeys(say(removed ? Word::DefaultsRestored : Word::DefaultsSaveFailed).c_str());
   refresh();
}

bool Hotkeys::handle(const Event& event)
{
   if (rows.empty())
      return false;
   switch (event.kind)
   {
      case RIB_RMLUI_ACTION_HOTKEYS_CANCEL:
         play_action_sound(event.kind);
         cancel_capture();
         return true;
      case RIB_RMLUI_ACTION_CONTROLS_BACK:
         /* Leaving ends a capture, and we choose the destination in Menu. */
         cancel_capture();
         return false;
      default:
         break;
   }
   Hotkey hotkey = Hotkey::Menu;
   const bool mine = event.kind == RIB_RMLUI_ACTION_HOTKEY_ADD
         || event.kind == RIB_RMLUI_ACTION_HOTKEY_REMOVE
         || event.kind == RIB_RMLUI_ACTION_HOTKEY_MODE
         || event.kind == RIB_RMLUI_ACTION_HOTKEYS_RESET;
   /* While we capture a binding, the player cannot press anything else. */
   if (capture.active)
      return true;
   if (!mine)
      return false;
   play_action_sound(event.kind);
   if (event.kind == RIB_RMLUI_ACTION_HOTKEYS_RESET)
      reset();
   else if (hotkey_named(event.id, hotkey))
   {
      if (event.kind == RIB_RMLUI_ACTION_HOTKEY_ADD)
         start_capture(hotkey);
      else if (event.kind == RIB_RMLUI_ACTION_HOTKEY_MODE)
      {
         const bool saved = bindings.next_mode(hotkey);
         status.set_hotkeys(saved ? say(Word::HotkeyModeSaved,
               {{"control", name(hotkey)}, {"mode", mode_word(hotkey)}}).c_str()
               : say(Word::BindingSaveFailed).c_str());
         refresh();
      }
      else
         remove(hotkey, event.slot);
   }
   return true;
}

void Hotkeys::screen_shown(bool showing)
{
   if (!showing)
   {
      cancel_capture();
      return;
   }
   /* We open the screen with the text from the design, not the last message. */
   status.set_hotkeys("");
   refresh();
}
}
