#pragma once
#include "document_contract.hpp"
#include <RmlUi/Core.h>
#include <algorithm>
#include <vector>

namespace rib {
namespace row_stop_detail {
struct StopBox
{
   Rml::Element *element;
   float left, top, right, bottom;
};

inline StopBox box_of(Rml::Element *element)
{
   const Rml::Vector2f at = element->GetAbsoluteOffset(Rml::BoxArea::Border);
   const Rml::Vector2f size = element->GetBox().GetSize(Rml::BoxArea::Border);
   return {element, at.x, at.y, at.x + size.x, at.y + size.y};
}

inline bool scrolls(Rml::Element *element)
{
   const auto& computed = element->GetComputedValues();
   const auto scrolling = [](Rml::Style::Overflow overflow) {
      return overflow == Rml::Style::Overflow::Scroll || overflow == Rml::Style::Overflow::Auto;
   };
   return scrolling(computed.overflow_x()) || scrolling(computed.overflow_y());
}

/* Every stop under `parent` but `from`, by the rules RmlUi navigates by. */
inline void collect_stops(Rml::Element *parent, Rml::Element *from, std::vector<StopBox>& stops)
{
   for (int index = 0; index < parent->GetNumChildren(); ++index)
   {
      Rml::Element *child = parent->GetChild(index);
      if (child == from)
         continue;
      if (!child->IsVisible() || child->GetComputedValues().focus() == Rml::Style::Focus::None)
         continue;
      if (child->GetComputedValues().tab_index() == Rml::Style::TabIndex::Auto)
         stops.push_back(box_of(child));
      else if (scrolls(child))
         continue;
      collect_stops(child, from, stops);
   }
}

/* Whether the design leaves this move to the layout. */
inline bool by_layout(Rml::Element *element, bool down)
{
   const Rml::Property *nav = element->GetLocalProperty(down ? Rml::PropertyId::NavDown : Rml::PropertyId::NavUp);
   if (!nav || nav->unit != Rml::Unit::KEYWORD)
      return false;
   const auto value = static_cast<Rml::Style::Nav>(nav->value.Get<int>());
   return value == Rml::Style::Nav::Auto || value == Rml::Style::Nav::Vertical;
}
}

/* The stop that Up or Down reaches from `from` inside its page of a list,
 * when the design leaves that move to the layout (`nav-up` or `nav-down` is
 * auto). In RmlUi any stop that overlaps `from` sideways wins over every
 * stop that does not, however far away it is, so the buttons below a list
 * could win over the next row of the list. We look in the page first, by
 * the same measure. Returns `from` when the move is RmlUi's to make: outside
 * a list, when the design decides it, or at the edge of the page. */
inline Rml::Element *row_stop(Rml::Element *from, bool down)
{
   using namespace row_stop_detail;
   while (from && from->GetComputedValues().tab_index() != Rml::Style::TabIndex::Auto)
      from = from->GetParentNode();
   if (!from || !by_layout(from, down))
      return from;
   Rml::Element *page = from->GetParentNode();
   while (page && !page->IsClassSet(document_contract::ListPage))
      page = page->GetParentNode();
   if (!page)
      return from;
   const StopBox source = box_of(from);
   std::vector<StopBox> stops;
   collect_stops(page, from, stops);
   /* RmlUi's measure: the distance down or up, and ten thousand times the
    * distance sideways outside the width of `from`. */
   const auto score = [&](const StopBox& stop) {
      const float along = down ? stop.top - source.bottom : source.top - stop.bottom;
      const float across = std::max(0.0f, stop.left - source.right) + std::max(0.0f, source.left - stop.right);
      return along < -0.5f ? -1.0f : along + 10000.0f * across;
   };
   const StopBox *best = nullptr;
   for (const StopBox& stop : stops)
      if (score(stop) >= 0.0f && (!best || score(stop) < score(*best)))
         best = &stop;
   return best ? best->element : from;
}
}
