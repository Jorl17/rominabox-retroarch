#pragma once
#include "document_contract.hpp"

#include <RmlUi/Core.h>
#include <RmlUi/Core/StringUtilities.h>
#include <algorithm>
#include <cstdio>
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

/* Focusable parts in document order, with the existing hidden/disabled
 * subtree pruning and fixed caller-provided id width. */
inline int focusable_ids(Rml::Element *document, const char *panel,
      char ids[][64], int capacity)
{
   if (!document || !panel || !ids || capacity <= 0)
      return 0;
   Rml::Element *root = document->GetElementById(panel);
   std::vector<std::string> found;
   walk(root, [&](Rml::Element *element) {
      if (display_none(element) || element->HasAttribute("disabled")
            || element->IsClassSet(document_contract::Disabled))
         return Walk::SkipChildren;
      const bool part = !element->IsClassSet(document_contract::VolumeArrow)
            && (element->IsClassSet(document_contract::Slider) || element->IsClassSet(document_contract::Toggle)
               || element->IsClassSet(document_contract::MenuAction)
               || element->GetTagName() == "input");
      if (part && !element->GetId().empty())
         found.push_back(element->GetId());
      return Walk::Continue;
   });
   int count = 0;
   for (const std::string& id : found)
   {
      if (count >= capacity)
         break;
      std::snprintf(ids[count], 64, "%s", id.c_str());
      ++count;
   }
   return count;
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
