#include "text_entry.hpp"
#include "text_host.h"
#include "elements.hpp"
#include <libretro.h>
#include <cstring>
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
      key->SetClass("menu-action", true);
      key->SetClass("text-key", true);
      key->SetAttribute("data-key", index);
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
   if (!panel_id.empty() && document.get_context())
      if (auto *focused = document.get_context()->GetFocusElement()) focused->Blur();
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
   if (key == RIB_KEY_UP || key == RIB_KEY_DOWN) {
      char ids[16][64];
      const int count = document.focusables(panel_id.c_str(), ids, 16);
      if (!count) return true;
      int index = 0;
      for (int at = 0; at < count; ++at)
         if (focused && focused->GetId() == ids[at]) index = at;
      index = (index + count + (key == RIB_KEY_UP ? -1 : 1)) % count;
      document.root()->GetElementById(ids[index])->Focus();
      return true;
   }
   if (key != RIB_KEY_OK && key != RIB_KEY_SELECT) return true;
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
   auto *focused = document.get_context()->GetFocusElement();
   document.mark_focused(panel_id.c_str(), focused ? focused->GetId().c_str() : nullptr);
   if (!keyboard_open()) return;
   if (auto *field = input(editing_id.c_str())) field->SetValue(rib_host_keyboard_value());
   auto *grid = document.root()->GetElementById(document_contract::TextKeyboardGrid);
   if (!grid) return;
   for (int index = 0; index < grid->GetNumChildren(); ++index) {
      auto *key = grid->GetChild(index);
      key->SetInnerRML(Rml::StringUtilities::EncodeRml(key_label(rib_host_keyboard_label(index))));
      key->SetClass("focused", index == rib_host_keyboard_focus());
   }
}
void TextEntry::ProcessEvent(Rml::Event& event)
{
   auto *target = event.GetTargetElement();
   while (target && !target->HasAttribute("data-key")) target = target->GetParentNode();
   if (target) rib_host_keyboard_choose(target->GetAttribute<unsigned>("data-key", RIB_KEYBOARD_KEYS));
}
bool TextEntry::physical(bool down, unsigned key, uint32_t character, uint16_t modifiers)
{
   if (panel_id.empty() || !document.root()) return false;
   auto *panel = document.root()->GetElementById(panel_id);
   if (!panel || hidden(panel)) return false;
   if (key == RETROK_TAB) return true;
   if (down && !keyboard_open() && (key == RETROK_UP || key == RETROK_DOWN))
      return controller(key == RETROK_UP ? RIB_KEY_UP : RIB_KEY_DOWN);
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
      context->ProcessKeyDown(mapped, flags);
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
