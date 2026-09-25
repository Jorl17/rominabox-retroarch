#include "text_entry.hpp"
#include "text_host.h"
#include "elements.hpp"
#include "../../../input/alt_enter_fullscreen.h"
#include "navigation.hpp"
#include "sounds.hpp"
#include <libretro.h>
#include <algorithm>
#include <cstring>
#include <vector>
#include <RmlUi/Core/Input.h>

namespace rib {
namespace {
Rml::Input::KeyIdentifier key_id(unsigned key)
{
   using namespace Rml::Input;
   if (key >= RETROK_a && key <= RETROK_z) return (KeyIdentifier)(KI_A + key - RETROK_a);
   if (key >= RETROK_0 && key <= RETROK_9) return (KeyIdentifier)(KI_0 + key - RETROK_0);
   switch (key) {
      case RETROK_BACKSPACE: return KI_BACK;
      case RETROK_DELETE: return KI_DELETE;
      case RETROK_TAB: return KI_TAB;
      case RETROK_RETURN: case RETROK_KP_ENTER: return KI_RETURN;
      case RETROK_LEFT: return KI_LEFT;
      case RETROK_RIGHT: return KI_RIGHT;
      case RETROK_UP: return KI_UP;
      case RETROK_DOWN: return KI_DOWN;
      case RETROK_HOME: return KI_HOME;
      case RETROK_END: return KI_END;
      case RETROK_ESCAPE: return KI_ESCAPE;
      case RETROK_SPACE: return KI_SPACE;
      default: return KI_UNKNOWN;
   }
}
const char *key_label(const char *label)
{
   if (!std::strcmp(label, "\xe2\x87\xa6") || !std::strcmp(label, "Bksp")) return "DEL";
   if (!std::strcmp(label, "\xe2\x8f\x8e") || !std::strcmp(label, "Enter")) return "OK";
   if (!std::strcmp(label, "\xe2\x87\xa7") || !std::strcmp(label, "Upper")) return "ABC";
   if (!std::strcmp(label, "\xe2\x87\xa9") || !std::strcmp(label, "Lower")) return "abc";
   if (!std::strcmp(label, "\xe2\x8a\x95") || !std::strcmp(label, "Next")) return "#+=";
   if (!std::strcmp(label, " ")) return "_";
   return label;
}
}
Rml::ElementFormControlInput *TextEntry::input(const char *id) const
{
   auto *element = document.root() ? document.root()->GetElementById(id) : nullptr;
   return dynamic_cast<Rml::ElementFormControlInput*>(element);
}
void TextEntry::bind()
{
   auto *grid = document.root() ? document.root()->GetElementById(document_contract::TextKeyboardGrid) : nullptr;
   if (!grid) return;
   grid->SetInnerRML("");
   for (unsigned index = 0; index < RIB_KEYBOARD_KEYS; ++index) {
      auto key = document.root()->CreateElement("button");
      key->SetClass(document_contract::MenuAction, true);
      key->SetClass(document_contract::TextKey, true);
      key->SetAttribute(document_contract::KeyAttribute, index);
      grid->AppendChild(std::move(key));
   }
   grid->AddEventListener(Rml::EventId::Click, this);
}
void TextEntry::enable(const char *panel, const char *submit, const char *cancel)
{
   panel_id = panel; submit_id = submit; cancel_id = cancel;
   rib_host_text_focus(true);
}
void TextEntry::disable()
{
   cancel_keyboard();
   /* Take the focus from a field of the form, but leave it where it is when it
    * has already moved to a screen we showed since. */
   if (!panel_id.empty() && document.root() && document.get_context())
      if (auto *focused = document.get_context()->GetFocusElement())
         if (auto *panel = document.root()->GetElementById(panel_id); panel && panel->Contains(focused))
            focused->Blur();
   panel_id.clear(); submit_id.clear(); cancel_id.clear();
   rib_host_text_focus(false);
}
bool TextEntry::keyboard_open() const { return !editing_id.empty(); }
void TextEntry::cancel_keyboard()
{
   if (auto *field = input(editing_id.c_str())) field->SetValue(original);
   rib_host_keyboard_end();
   editing_id.clear(); original.clear();
   document.set_shown(document_contract::TextKeyboard, false);
}
void TextEntry::complete(void *context, const char *value)
{
   auto& self = *static_cast<TextEntry*>(context);
   if (auto *field = self.input(self.editing_id.c_str())) field->SetValue(value);
   self.editing_id.clear(); self.original.clear();
   self.document.set_shown(document_contract::TextKeyboard, false);
}
bool TextEntry::controller(rib_key key)
{
   if (panel_id.empty()) return false;
   if (keyboard_open()) return true;
   auto *focused = document.get_context()->GetFocusElement();
   /* The arrows move by the layout of the form, as everywhere in the menu, but
    * the field with the caret gets Left and Right first. */
   if (navigate(document.get_context(), key) || (key != RIB_KEY_OK && key != RIB_KEY_SELECT))
      return true;
   auto *field = dynamic_cast<Rml::ElementFormControlInput*>(focused);
   if (!field || hidden(field)) {
      if (focused && focused->GetTagName() == "button") focused->Click();
      return true;
   }
   editing_id = focused->GetId(); original = field->GetValue();
   if (!rib_host_keyboard_begin(original.c_str(), complete, this)) {
      editing_id.clear(); original.clear(); return false;
   }
   field->Focus();
   document.set_shown(document_contract::TextKeyboard, true);
   return true;
}
void TextEntry::update()
{
   if (panel_id.empty()) return;
   rib_host_text_focus(true);
   if (!keyboard_open()) return;
   if (auto *field = input(editing_id.c_str())) field->SetValue(rib_host_keyboard_value());
   auto *grid = document.root()->GetElementById(document_contract::TextKeyboardGrid);
   if (!grid) return;
   for (int index = 0; index < grid->GetNumChildren(); ++index) {
      auto *key = grid->GetChild(index);
      key->SetInnerRML(Rml::StringUtilities::EncodeRml(key_label(rib_host_keyboard_label(index))));
      key->SetClass(document_contract::Focused, index == rib_host_keyboard_focus());
   }
}
void TextEntry::ProcessEvent(Rml::Event& event)
{
   auto *target = event.GetTargetElement();
   while (target && !target->HasAttribute(document_contract::KeyAttribute)) target = target->GetParentNode();
   if (target) rib_host_keyboard_choose(target->GetAttribute<unsigned>(document_contract::KeyAttribute, RIB_KEYBOARD_KEYS));
}
bool TextEntry::physical(bool down, unsigned key, uint32_t character, uint16_t modifiers)
{
   if (panel_id.empty() || !document.root()) return false;
   auto *panel = document.root()->GetElementById(panel_id);
   if (!panel || hidden(panel)) return false;
   if (alt_enter_is_chord(key, modifiers)) return false;
   if (key == RETROK_TAB) {
      /* With Tab the player moves through the fields and buttons of the form,
       * in document order, and back with Shift+Tab, never out of the form. */
      if (down && !keyboard_open()) {
         std::vector<Rml::Element*> stops;
         walk(panel, [&](Rml::Element *element) {
            if (display_none(element)) return Walk::SkipChildren;
            if (element->GetComputedValues().tab_index() == Rml::Style::TabIndex::Auto &&
                  !element->HasAttribute("disabled"))
               stops.push_back(element);
            return Walk::Continue;
         });
         if (!stops.empty()) {
            const bool back = modifiers & RETROKMOD_SHIFT;
            const auto at = std::find(stops.begin(), stops.end(), document.get_context()->GetFocusElement());
            const int count = (int)stops.size();
            const int next = at == stops.end() ? (back ? count - 1 : 0)
                  : ((int)(at - stops.begin()) + (back ? count - 1 : 1)) % count;
            if (stops[next]->Focus(true)) play_move_sound(back);
         }
      }
      return true;
   }
   /* While the form has the keyboard, Up and Down move in the menu. We pass
    * Left and Right with their modifiers to the field below, and at its edge
    * we pass them on. */
   if (down && !keyboard_open() && (key == RETROK_UP || key == RETROK_DOWN)) {
      navigate(document.get_context(), key == RETROK_UP ? RIB_KEY_UP : RIB_KEY_DOWN);
      return true;
   }
   if (down && key == RETROK_ESCAPE) {
      if (keyboard_open()) cancel_keyboard(); else document.click_element(cancel_id.c_str());
      return true;
   }
   if (keyboard_open() && (character == '\n' || character == '\r')) return false;
   if (down && !keyboard_open() && (key == RETROK_RETURN || key == RETROK_KP_ENTER)) {
      auto *focused = document.get_context()->GetFocusElement();
      if (focused && focused->GetTagName() == "button") focused->Click();
      else document.click_element(submit_id.c_str());
      return true;
   }
   auto *context = document.get_context();
   if (keyboard_open()) if (auto *field = input(editing_id.c_str())) field->Focus();
   int flags = 0;
   if (modifiers & RETROKMOD_SHIFT) flags |= Rml::Input::KM_SHIFT;
   if (modifiers & (RETROKMOD_CTRL | RETROKMOD_META)) flags |= Rml::Input::KM_CTRL;
   if (modifiers & RETROKMOD_ALT) flags |= Rml::Input::KM_ALT;
   // We get the committed codepoint of the layout from RetroArch. AltGr may
   // come with Ctrl+Alt, so we do not let it trigger shortcuts such as Ctrl-A.
   const bool altgr_text = character >= 32 && character != 127 &&
         (modifiers & RETROKMOD_CTRL) && (modifiers & RETROKMOD_ALT) && !(modifiers & RETROKMOD_META);
   if (altgr_text) flags &= ~(Rml::Input::KM_CTRL | Rml::Input::KM_ALT);
   auto mapped = key_id(key);
   if (keyboard_open() && character == 127) mapped = Rml::Input::KI_BACK;
   if (down) {
      auto *before = context->GetFocusElement();
      context->ProcessKeyDown(mapped, flags);
      if ((key == RETROK_LEFT || key == RETROK_RIGHT) && context->GetFocusElement() != before)
         play_move_sound(key == RETROK_LEFT);
      if (character >= 32 && character != 127 && !(flags & Rml::Input::KM_CTRL))
         context->ProcessTextInput((Rml::Character)character);
   } else context->ProcessKeyUp(mapped, flags);
   if (keyboard_open()) if (auto *field = input(editing_id.c_str())) rib_host_keyboard_replace(field->GetValue().c_str());
   return true;
}
}

namespace rib {
bool TextEntry::begin_native_input()
{
   if (panel_id.empty() || !document.root()) return false;
   if (keyboard_open()) {
      // Typing on the physical keyboard takes over the current value, so we
      // never edit one field from two editors during text composition.
      if (auto *field = input(editing_id.c_str())) field->SetValue(rib_host_keyboard_value());
      rib_host_keyboard_end();
      editing_id.clear(); original.clear();
      document.set_shown(document_contract::TextKeyboard, false);
   }
   return true;
}
}
