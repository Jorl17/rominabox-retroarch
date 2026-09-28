#include "menu_controls.hpp"

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
std::string row_id(MenuAction action)
{
   return std::string(document_contract::MenuControlPrefix) + menu_action_id(action);
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

void MenuControls::bind()
{
   rows.clear();
   Rml::Element *root = document.root();
   if (!root)
      return;
   const MenuAction *actions = menu_actions();
   for (size_t index = 0; index < kMenuActionCount; ++index)
   {
      Row row;
      row.action = actions[index];
      const std::string base = row_id(row.action);
      row.add = base + document_contract::AddSuffix;
      row.label = base + document_contract::LabelSuffix;
      Rml::Element *add = root->GetElementById(row.add);
      /* When the game has no MENU CONTROLS screen, there are no rows. */
      if (!add)
         continue;
      listen(add, intents, hovered,
            Event::menu_control(RIB_RMLUI_ACTION_MENU_CONTROL_ADD, menu_action_id(row.action)));
      for (int chip = 1;; ++chip)
      {
         const std::string id = base + "-" + std::to_string(chip);
         Rml::Element *element = root->GetElementById(id);
         if (!element)
            break;
         listen(element, intents, hovered, Event::menu_control(
               RIB_RMLUI_ACTION_MENU_CONTROL_REMOVE, menu_action_id(row.action), chip));
         row.chips.push_back(id);
      }
      rows.push_back(row);
   }
   if (rows.empty())
      return;
   listen(root->GetElementById(document_contract::MenuControlsReset), intents, hovered,
         Event(RIB_RMLUI_ACTION_MENU_CONTROLS_RESET));
   listen(root->GetElementById(document_contract::MenuControlsCancel), intents, hovered,
         Event(RIB_RMLUI_ACTION_MENU_CONTROLS_CANCEL));
   if (Rml::Element *back = root->GetElementById(document_contract::MenuControlsBack))
      back->AddEventListener(Rml::EventId::Click, new ReturnListener(intents));
   refresh();
}

const MenuControls::Row *MenuControls::row(MenuAction action) const
{
   for (const Row& each : rows)
      if (each.action == action)
         return &each;
   return nullptr;
}

/* The name of the action on its row, which is the word from the design. */
std::string MenuControls::name(MenuAction action) const
{
   const Row *shown = row(action);
   Rml::Element *label = shown && document.root()
         ? document.root()->GetElementById(shown->label) : nullptr;
   std::string text = label ? label->GetInnerRML() : std::string();
   if (text.empty())
      for (const char *at = menu_action_id(action); *at; ++at)
         text += (char)std::toupper((unsigned char)*at);
   return text;
}

void MenuControls::refresh()
{
   for (const Row& shown : rows)
   {
      const std::vector<MenuBinding>& list = bindings.of(shown.action);
      for (size_t chip = 0; chip < shown.chips.size(); ++chip)
      {
         const char *id = shown.chips[chip].c_str();
         const bool used = chip < list.size();
         document.set_shown(id, used);
         if (!used)
            continue;
         document.set_element_text(id, bindings.words(list[chip]).c_str());
         document.set_class(id, document_contract::ChipKey,
               list[chip].kind == MenuBinding::Kind::Key);
         document.set_class(id, document_contract::ChipPad,
               list[chip].kind == MenuBinding::Kind::Pad);
      }
      document.set_disabled(shown.add.c_str(), list.size() >= shown.chips.size());
      document.set_class(shown.add.c_str(), document_contract::Capturing,
            capture.active && capture.action == shown.action);
   }
   if (rows.empty())
      return;
   document.set_class(document_contract::MenuControlsCancel, document_contract::Capturing,
         capture.active);
   document.set_shown(document_contract::MenuControlsCancel, capture.active);
}

std::string MenuControls::outcome(const MenuBindings::Change& change) const
{
   using Outcome = MenuBindings::Outcome;
   const std::string other = name(change.other);
   switch (change.outcome)
   {
      case Outcome::Unchanged: return say(Word::BindingUnchanged);
      case Outcome::Full: return say(Word::BindingNoRoom, {{"control", other}});
      case Outcome::NeedsBinding: return say(Word::BindingNeeded, {{"control", other}});
      case Outcome::NeedsKey: return say(Word::KeyNeeded, {{"control", other}});
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

void MenuControls::start_capture(MenuAction action)
{
   const Row *shown = row(action);
   if (!shown)
      return;
   if (!rib_host_capture_input_start(RIB_CONTROL_CAPTURE_SECONDS))
   {
      status.set_menu_controls(say(Word::CaptureFailed).c_str());
      return;
   }
   capture.active = true;
   capture.action = action;
   capture.ignore_pointer = true;
   focus.set(shown->add.c_str());
   status.set_menu_controls(say(Word::CaptureCountdown, {{"control", name(action)},
         {"seconds", std::to_string(RIB_CONTROL_CAPTURE_SECONDS)}}).c_str());
   screens.set_footer_hint(say(Word::CancelHint).c_str());
   refresh();
}

void MenuControls::end_capture(const std::string& words)
{
   capture.active = false;
   status.set_menu_controls(words.c_str());
   screens.restore_footer();
   refresh();
   if (const Row *shown = row(capture.action))
      focus.set(shown->add.c_str());
}

void MenuControls::cancel_capture()
{
   if (!capture.active)
      return;
   rib_host_capture_cancel();
   end_capture(say(Word::BindingUnchanged));
}

void MenuControls::ignore_pointer(bool pressed)
{
   if (!capture.active)
      return;
   if (pressed && hovered.kind == RIB_RMLUI_ACTION_MENU_CONTROLS_CANCEL)
      capture.ignore_pointer = true;
   else if (!pressed)
      capture.ignore_pointer = false;
}

void MenuControls::poll_capture()
{
   if (!capture.active)
      return;
   float remaining = 0.0f;
   switch (rib_host_capture_poll(!capture.ignore_pointer, &remaining))
   {
      case RIB_CAPTURE_CAPTURED:
      {
         char text[128];
         rib_host_captured_input(text, sizeof(text));
         MenuBinding binding;
         if (!*text || !read_menu_binding(text, binding))
            end_capture(say(Word::BindingUnusable));
         else if (cancels_capture(binding))
            end_capture(say(Word::BindingUnchanged));
         else
         {
            std::vector<size_t> room;
            const MenuAction *actions = menu_actions();
            for (size_t index = 0; index < kMenuActionCount; ++index)
            {
               const Row *shown = row(actions[index]);
               room.push_back(shown ? shown->chips.size() : 0);
            }
            end_capture(outcome(bindings.add(capture.action, binding, room)));
         }
         break;
      }
      case RIB_CAPTURE_TIMED_OUT:
         end_capture(say(Word::CaptureTimeout));
         break;
      default:
         status.set_menu_controls(say(Word::CaptureCountdown, {{"control", name(capture.action)},
               {"seconds", std::to_string((unsigned)(remaining + 0.999f))}}).c_str());
         break;
   }
}

void MenuControls::remove(MenuAction action, int chip)
{
   const Row *shown = row(action);
   if (!shown || chip < 1)
      return;
   status.set_menu_controls(outcome(bindings.remove(action, (size_t)(chip - 1))).c_str());
   refresh();
   /* We keep the focus where it was, or move it to the next binding, to the
    * last one left, or to + when none is left. */
   const size_t left = bindings.of(action).size();
   if (left == 0)
      focus.set(shown->add.c_str());
   else if ((size_t)chip > left)
      focus.set(shown->chips[left - 1].c_str());
}

void MenuControls::reset()
{
   cancel_capture();
   const bool removed = bindings.reset();
   status.set_menu_controls(say(removed ? Word::DefaultsRestored : Word::DefaultsSaveFailed).c_str());
   refresh();
}

bool MenuControls::handle(const Event& event)
{
   if (rows.empty())
      return false;
   switch (event.kind)
   {
      case RIB_RMLUI_ACTION_MENU_CONTROLS_CANCEL:
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
   MenuAction action = MenuAction::Menu;
   const bool mine = event.kind == RIB_RMLUI_ACTION_MENU_CONTROL_ADD
         || event.kind == RIB_RMLUI_ACTION_MENU_CONTROL_REMOVE
         || event.kind == RIB_RMLUI_ACTION_MENU_CONTROLS_RESET;
   /* While we capture a binding, the player cannot press anything else. */
   if (capture.active)
      return true;
   if (!mine)
      return false;
   play_action_sound(event.kind);
   if (event.kind == RIB_RMLUI_ACTION_MENU_CONTROLS_RESET)
      reset();
   else if (menu_action_named(event.id, action))
   {
      if (event.kind == RIB_RMLUI_ACTION_MENU_CONTROL_ADD)
         start_capture(action);
      else
         remove(action, event.slot);
   }
   return true;
}

void MenuControls::screen_shown(bool showing)
{
   if (!showing)
   {
      cancel_capture();
      return;
   }
   /* We open the screen with the text from the design, not the last message. */
   status.set_menu_controls("");
   refresh();
}
}
