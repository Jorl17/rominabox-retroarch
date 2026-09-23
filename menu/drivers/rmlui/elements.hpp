#pragma once

#include <RmlUi/Core.h>
#include <algorithm>
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
