#pragma once
#include "document_contract.hpp"

#include <RmlUi/Core.h>
#include <RmlUi/Core/StringUtilities.h>
#include <algorithm>
#include <string>
#include <vector>

namespace rib
{
enum class Walk { Continue, SkipChildren, Stop };

/* Pre-order document traversal. In a visitor we can prune hidden branches or
 * stop at the first match, with no change to the element order for navigation. */
template<typename Visit>
bool walk(Rml::Element *element, const Visit &visit)
{
   if (!element)
      return true;
   const Walk next = visit(element);
   if (next == Walk::Stop)
      return false;
   if (next == Walk::SkipChildren)
      return true;
   const int count = element->GetNumChildren();
   for (int index = 0; index < count; ++index)
      if (!walk(element->GetChild(index), visit))
         return false;
   return true;
}

inline bool display_none(Rml::Element *element)
{
   const Rml::Property *property = element ? element->GetProperty("display") : nullptr;
   return property && property->ToString() == "none";
}

inline bool hidden(Rml::Element *element)
{
   for (auto *at = element; at; at = at->GetParentNode())
      if (display_none(at))
         return true;
   return false;
}

/* A field for text editing in RmlUi: a text area, or an input of type text
 * or password, marked with the class for its type. */
inline bool edits_text(Rml::Element *element)
{
   return dynamic_cast<Rml::ElementFormControlTextArea*>(element)
         || (dynamic_cast<Rml::ElementFormControlInput*>(element)
            && (element->IsClassSet("text") || element->IsClassSet("password")));
}

/* The writes from the menu to the document. Whatever we write is laid out or
 * drawn again in RmlUi, even the same value, so we write only changes, and
 * there is no work while nobody touches the menu. */
inline void show(Rml::Element *element, bool shown)
{
   if (!element)
      return;
   const Rml::Property *own = element->GetLocalProperty("display");
   const bool hidden_here = own && own->ToString() == "none";
   if (shown)
      element->RemoveProperty("display");
   else if (!hidden_here)
      element->SetProperty("display", "none");
}

/* `text` as the element's words, not markup. */
inline void write_text(Rml::Element *element, const std::string& text)
{
   if (!element)
      return;
   const Rml::String encoded = Rml::StringUtilities::EncodeRml(text);
   if (element->GetInnerRML() != encoded)
      element->SetInnerRML(encoded);
}

/* The words of a hint, such as "[ESC]  BACK". We write each word in brackets,
 * the name of a key, as a separate element (`hint-key`) for the design to
 * style, and the rest as it is. A hotkey in braces, "{back}" or with words to
 * show until we read its binding, "{back:Escape}", is such an element marked
 * data-binding="back" (document_contract.inc, BindingAttribute). */
inline void write_hint(Rml::Element *element, const std::string& hint)
{
   if (!element)
      return;
   Rml::String markup;
   for (size_t at = 0; at < hint.size();)
   {
      const size_t open = hint.find_first_of("[{", at);
      const char closing = open == std::string::npos ? ']' : (hint[open] == '[' ? ']' : '}');
      const size_t close = open == std::string::npos ? std::string::npos : hint.find(closing, open);
      if (close == std::string::npos)
      {
         markup += Rml::StringUtilities::EncodeRml(hint.substr(at));
         break;
      }
      markup += Rml::StringUtilities::EncodeRml(hint.substr(at, open - at));
      std::string inside = hint.substr(open + 1, close - open - 1);
      std::string binding;
      if (closing == '}')
      {
         const size_t colon = inside.find(':');
         binding = inside.substr(0, colon);
         inside = colon == std::string::npos ? std::string() : inside.substr(colon + 1);
      }
      markup += std::string("<span class=\"") + document_contract::HintKey + "\"";
      if (!binding.empty())
         markup += std::string(" ") + document_contract::BindingAttribute + "=\""
               + Rml::StringUtilities::EncodeRml(binding) + "\"";
      markup += ">" + Rml::StringUtilities::EncodeRml(inside) + "</span>";
      at = close + 1;
   }
   if (element->GetInnerRML() != markup)
      element->SetInnerRML(markup);
}

/* An element's words as a reader has them: its text and its children's,
 * without the markup a design styles them with. */
inline std::string words_of(Rml::Element *element)
{
   if (!element)
      return {};
   if (auto *text = rmlui_dynamic_cast<Rml::ElementText *>(element))
      return text->GetText();
   std::string words;
   for (int index = 0; index < element->GetNumChildren(); ++index)
      words += words_of(element->GetChild(index));
   return words;
}

/* We always set both the `disabled` class, for the design, and the disabled
 * attribute, for RmlUi and the listeners. */
inline void disable(Rml::Element *element, bool disabled)
{
   if (!element)
      return;
   element->SetClass(document_contract::Disabled, disabled);
   if (!disabled)
      element->RemoveAttribute("disabled");
   else if (!element->HasAttribute("disabled"))
      element->SetAttribute("disabled", "disabled");
}

inline void collect(Rml::Element *root, const char *class_name,
      std::vector<Rml::Element*> &out)
{
   walk(root, [&](Rml::Element *element) {
      if (element->IsClassSet(class_name))
         out.push_back(element);
      return Walk::Continue;
   });
}

inline Rml::Element *find_class(Rml::Element *root, const char *class_name,
      bool require_id = false)
{
   Rml::Element *found = nullptr;
   walk(root, [&](Rml::Element *element) {
      if (element->IsClassSet(class_name) && (!require_id || !element->GetId().empty()))
      {
         found = element;
         return Walk::Stop;
      }
      return Walk::Continue;
   });
   return found;
}

inline float specified_dp(Rml::Element *element, const char *name, Rml::Context *context)
{
   const Rml::Property *property = element ? element->GetProperty(name) : nullptr;
   if (!property)
      return 0.f;
   if (property->unit != Rml::Unit::DP && property->unit != Rml::Unit::PX
         && property->unit != Rml::Unit::NUMBER)
      return 0.f;
   const float value = property->value.Get<float>();
   if (property->unit != Rml::Unit::PX || !context)
      return value;
   return value / std::max(context->GetDensityIndependentPixelRatio(), 0.1f);
}
}
