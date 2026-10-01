#include "document_contract.hpp"
#include "lists.hpp"

#include "binds_popup.hpp"
#include "document.hpp"
#include "elements.hpp"
#include "events.h"
#include "paging.hpp"
#include "words.hpp"

#include <RmlUi/Core/ElementUtilities.h>
#include <RmlUi/Core/StringUtilities.h>

#include <algorithm>
#include <cstdio>
#include <utility>

namespace rib {
namespace {
/* The player chose a row or turned a page by `direction`, -1 back or 1 on. */
class ListListener : public Rml::EventListener
{
public:
   ListListener(EventQueue& events, int direction)
      : events(events), direction(direction) {}
   void ProcessEvent(Rml::Event& event) override
   {
      auto *element = event.GetCurrentElement();
      if (!element || element->HasAttribute("disabled") ||
            element->IsClassSet(document_contract::Disabled))
         return;
      if (!direction)
      {
         events.push({RIB_RMLUI_ACTION_LIST_CHOOSE, std::string(element->GetId())});
         return;
      }
      Event page(RIB_RMLUI_ACTION_LIST_PAGE);
      page.direction = direction;
      events.push(page);
   }
   void OnDetach(Rml::Element*) override { delete this; }
private:
   EventQueue& events;
   int direction;
};

int utf8_len(unsigned char lead)
{
   if ((lead & 0x80) == 0) return 1;
   if ((lead & 0xe0) == 0xc0) return 2;
   if ((lead & 0xf0) == 0xe0) return 3;
   if ((lead & 0xf8) == 0xf0) return 4;
   return 1;
}

int chars(const std::string& text)
{
   int count = 0;
   for (size_t index = 0; index < text.size(); )
   {
      int length = utf8_len((unsigned char)text[index]);
      if (index + (size_t)length > text.size()) length = 1;
      index += (size_t)length;
      ++count;
   }
   return count;
}

std::string slice(const std::string& text, int from, int count)
{
   std::string out;
   int seen = 0;
   for (size_t index = 0; index < text.size() && seen < from + count; )
   {
      int length = utf8_len((unsigned char)text[index]);
      if (index + (size_t)length > text.size()) length = 1;
      if (seen >= from) out.append(text, index, (size_t)length);
      index += (size_t)length;
      ++seen;
   }
   return out;
}
}

void Lists::wire_lists(Rml::Element *scope)
{
   if (!document.root()) return;
   if (!scope) scope = document.root();
   std::vector<Rml::Element*> rows;
   collect(scope, document_contract::ListRow, rows);
   for (auto *row : rows)
      if (!row->GetParentNode()->IsClassSet(document_contract::ListPrototype))
         row->AddEventListener(Rml::EventId::Click, new ListListener(events, 0));
   std::vector<Rml::Element*> previous;
   collect(scope, document_contract::ListPagerPrev, previous);
   for (auto *button : previous)
      button->AddEventListener(Rml::EventId::Click, new ListListener(events, -1));
   std::vector<Rml::Element*> next;
   collect(scope, document_contract::ListPagerNext, next);
   for (auto *button : next)
      button->AddEventListener(Rml::EventId::Click, new ListListener(events, 1));
}

Rml::Element *Lists::visible_list() const
{
   if (!document.root()) return nullptr;
   for (auto *list : paging::lists_in(document.root()))
      if (!hidden(list)) return list;
   return nullptr;
}

int Lists::turn_list_page(int delta, Rml::Element *list) const
{
   return paging::turn(list ? list : visible_list(), delta);
}

Rml::Element *Lists::shown_page(Rml::Element *list) const
{
   return paging::shown(list ? list : visible_list());
}

void Lists::select_in(Rml::Element *list, const char *row_id,
      const char *on, const char *off) const
{
   std::vector<Rml::Element*> rows;
   collect(list, document_contract::ListRow, rows);
   for (auto *row : rows)
   {
      const bool selected = row_id && row->GetId() == row_id;
      row->SetClass(document_contract::Selected, selected);
      document.set_element_text((row->GetId() + document_contract::StateSuffix).c_str(),
            selected ? on : off);
   }
}

void Lists::mark_row(const char *id, const char *on, const char *off) const
{
   auto *list = visible_list();
   if (document.root() && list) select_in(list, id, on, off);
}

void Lists::select_row(const char *list_id, const char *row_id,
      const char *on, const char *off) const
{
   if (document.root() && list_id)
      select_in(list_element(list_id), row_id, on, off);
}

void Lists::set_row_text(const char *id, const char *title,
      const char *detail, const char *state) const
{
   if (!id || !*id) return;
   const std::string row(id);
   document.set_element_text((row + document_contract::TitleSuffix).c_str(), title);
   document.set_element_text((row + document_contract::DetailSuffix).c_str(), detail);
   document.set_element_text((row + document_contract::StateSuffix).c_str(), state);
}

float Lists::block_dp(Rml::Element *element) const
{
   for (auto *cursor = element; cursor; cursor = cursor->GetParentNode())
   {
      const float width = specified_dp(cursor, "width", document.get_context());
      if (width > 0.f) return width;
   }
   return 0.f;
}

void Lists::fit_row_title(const char *id, const char *text) const
{
   if (!id || !*id) return;
   const std::string title_id = std::string(id) + document_contract::TitleSuffix;
   const std::string source = text ? text : "";
   auto *element = document.root() ? document.root()->GetElementById(title_id) : nullptr;
   if (!element || !document.get_context())
   {
      document.set_element_text(title_id.c_str(), source.c_str());
      return;
   }
   document.get_context()->Update();
   const float density = std::max(document.get_context()->GetDensityIndependentPixelRatio(), 0.1f);
   const float block = block_dp(element);
   const float limit_dp = block
         - specified_dp(element, "left", document.get_context())
         - specified_dp(element, "right", document.get_context());
   const float limit_px = limit_dp * density;
   auto width_of = [&](const std::string& value) {
      return (float)Rml::ElementUtilities::GetStringWidth(element, Rml::String(value));
   };
   if (limit_px <= 1.f || width_of(source) <= limit_px)
   {
      write_text(element, source);
      return;
   }
   /* We cut the middle of the title to keep the disc number at its end, and
    * measure in the font of the title, not in characters. */
   const std::string mark = "\u2026";
   const int total = chars(source);
   int best = 0, low = 0, high = total;
   while (low <= high)
   {
      const int mid = (low + high) / 2;
      const int head = mid / 2;
      const int tail = mid - head;
      const std::string candidate = slice(source, 0, head) + mark
            + slice(source, total - tail, tail);
      if (width_of(candidate) <= limit_px)
      {
         best = mid;
         low = mid + 1;
      }
      else high = mid - 1;
   }
   const int head = best / 2;
   const int tail = best - head;
   write_text(element, slice(source, 0, head) + mark + slice(source, total - tail, tail));
}

Rml::Element *Lists::list_element(const char *list_id) const
{
   if (!document.root() || !list_id || !*list_id) return nullptr;
   return document.root()->GetElementById(list_id);
}

int Lists::rows_in(const char *list_id) const
{
   std::vector<Rml::Element*> rows;
   collect(list_element(list_id), document_contract::ListRow, rows);
   return (int)rows.size();
}

std::string Lists::row_in(const char *list_id, int index) const
{
   std::vector<Rml::Element*> rows;
   collect(list_element(list_id), document_contract::ListRow, rows);
   return index >= 0 && index < (int)rows.size() ? rows[index]->GetId() : std::string();
}

void Lists::retarget_pages(const char *list_id, const char *keep_row) const
{
   auto *list = list_element(list_id);
   if (!list) return;
   paging::split(list, paging::rows(list), 0, keep_row ? list->GetElementById(keep_row) : nullptr);
}

void Lists::place_list(const char *list_id, const char *anchor_id, int width_dp) const
{
   place_binds_popup(document.root(), document.get_context(),
         list_id, anchor_id, width_dp);
}
}
