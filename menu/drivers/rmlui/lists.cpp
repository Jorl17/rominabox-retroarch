#include "lists.hpp"

#include "binds_popup.hpp"
#include "document.hpp"
#include "elements.hpp"
#include "events.h"

#include <RmlUi/Core/ElementUtilities.h>
#include <RmlUi/Core/StringUtilities.h>

#include <algorithm>
#include <cstdio>
#include <utility>

namespace rib {
namespace {
bool under_hidden_page(Rml::Element *element)
{
   for (auto *cursor = element; cursor; cursor = cursor->GetParentNode())
      if (cursor->IsClassSet("list-page") && display_none(cursor))
         return true;
   return false;
}

class ListListener : public Rml::EventListener
{
public:
   enum Kind { Choose, Page };
   ListListener(EventQueue& events, Kind kind, std::string page)
      : events(events), kind(kind), page(std::move(page)) {}
   void ProcessEvent(Rml::Event& event) override
   {
      auto *element = event.GetCurrentElement();
      if (!element || element->HasAttribute("disabled") ||
            element->IsClassSet("disabled"))
         return;
      events.push({kind == Choose ? RIB_RMLUI_ACTION_LIST_CHOOSE
            : RIB_RMLUI_ACTION_LIST_PAGE,
            kind == Choose ? std::string(element->GetId()) : page});
   }
   void OnDetach(Rml::Element*) override { delete this; }
private:
   EventQueue& events;
   Kind kind;
   std::string page;
};

class ToggleListener : public Rml::EventListener
{
public:
   explicit ToggleListener(EventQueue& events) : events(events) {}
   void ProcessEvent(Rml::Event& event) override
   {
      auto *element = event.GetCurrentElement();
      if (!element || element->HasAttribute("disabled") ||
            element->IsClassSet("disabled"))
         return;
      events.push({RIB_RMLUI_ACTION_TOGGLE, element->GetId()});
   }
   void OnDetach(Rml::Element*) override { delete this; }
private:
   EventQueue& events;
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

void Lists::wire_lists()
{
   if (!document.root()) return;
   std::vector<Rml::Element*> rows;
   collect(document.root(), "list-row", rows);
   for (auto *row : rows)
      row->AddEventListener(Rml::EventId::Click,
            new ListListener(events, ListListener::Choose, ""));
   std::vector<Rml::Element*> previous;
   collect(document.root(), "list-pager-prev", previous);
   for (auto *button : previous)
      button->AddEventListener(Rml::EventId::Click,
            new ListListener(events, ListListener::Page, "prev"));
   std::vector<Rml::Element*> next;
   collect(document.root(), "list-pager-next", next);
   for (auto *button : next)
      button->AddEventListener(Rml::EventId::Click,
            new ListListener(events, ListListener::Page, "next"));
}

void Lists::wire_toggles()
{
   if (!document.root()) return;
   std::vector<Rml::Element*> toggles;
   collect(document.root(), "list-toggle", toggles);
   for (auto *toggle : toggles)
      toggle->AddEventListener(Rml::EventId::Click, new ToggleListener(events));
}

void Lists::set_toggle(const char *id, const char *state, bool on)
{
   if (!document.root() || !id || !*id) return;
   if (auto *toggle = document.root()->GetElementById(id))
      toggle->SetClass("on", on);
   set_text(std::string(id) + "-state", state);
}

Rml::Element *Lists::visible_list() const
{
   if (!document.root()) return nullptr;
   std::vector<Rml::Element*> lists;
   collect(document.root(), "list", lists);
   for (auto *list : lists)
      if (!hidden(list)) return list;
   return nullptr;
}

void Lists::visible_rows(std::vector<Rml::Element*> &rows) const
{
   rows.clear();
   auto *list = visible_list();
   if (!list) return;
   std::vector<Rml::Element*> all;
   collect(list, "list-row", all);
   for (auto *row : all)
      if (!under_hidden_page(row) && !display_none(row)) rows.push_back(row);
}

Rml::Element *Lists::visible_panel() const
{
   if (!document.root()) return nullptr;
   std::vector<Rml::Element*> panels;
   collect(document.root(), "screen-panel", panels);
   for (auto *panel : panels)
      if (!display_none(panel)) return panel;
   return nullptr;
}

void Lists::visible_controls(std::vector<Rml::Element*> &out) const
{
   out.clear();
   auto *panel = visible_panel();
   if (!panel) return;
   for (const char *name : {"option-entry", "list-toggle", "list-back", "options-back"})
      collect(panel, name, out);
}

bool Lists::click_screen_back() const
{
   auto *panel = visible_panel();
   if (!panel) return false;
   std::vector<Rml::Element*> back;
   collect(panel, "list-back", back);
   collect(panel, "options-back", back);
   if (back.empty()) return false;
   const auto& id = back.front()->GetId();
   if (id.empty()) return false;
   auto *element = document.root()->GetElementById(id);
   if (!element) return false;
   /* Click through the element listener, so a disabled element stays inert. */
   element->Click();
   return true;
}

int Lists::visible_row_count() const
{
   std::vector<Rml::Element*> rows;
   visible_rows(rows);
   return (int)rows.size();
}

void Lists::focus_list_row(int index) const
{
   if (!document.root()) return;
   std::vector<Rml::Element*> all;
   collect(document.root(), "list-row", all);
   for (auto *row : all) row->SetClass("focused", false);
   std::vector<Rml::Element*> rows;
   visible_rows(rows);
   if (index >= 0 && index < (int)rows.size())
      rows[index]->SetClass("focused", true);
}

int Lists::list_control_count() const
{
   std::vector<Rml::Element*> controls;
   visible_controls(controls);
   return (int)controls.size();
}

const char *Lists::list_control_id(int index)
{
   std::vector<Rml::Element*> controls;
   visible_controls(controls);
   control_id_buffer.clear();
   if (index >= 0 && index < (int)controls.size())
      control_id_buffer = controls[index]->GetId();
   return control_id_buffer.c_str();
}

void Lists::focus_list_control(int index) const
{
   std::vector<Rml::Element*> controls;
   visible_controls(controls);
   for (size_t at = 0; at < controls.size(); ++at)
      controls[at]->SetClass("focused", (int)at == index);
}

const char *Lists::list_row_id(int index)
{
   std::vector<Rml::Element*> rows;
   visible_rows(rows);
   row_id_buffer.clear();
   if (index >= 0 && index < (int)rows.size())
      row_id_buffer = rows[index]->GetId();
   return row_id_buffer.c_str();
}

int Lists::hovered_list_row() const
{
   if (!document.get_context() || !document.root()) return -1;
   auto *cursor = document.get_context()->GetHoverElement();
   Rml::Element *row = nullptr;
   for (; cursor; cursor = cursor->GetParentNode())
      if (cursor->IsClassSet("list-row")) { row = cursor; break; }
   if (!row) return -1;
   std::vector<Rml::Element*> rows;
   visible_rows(rows);
   for (int index = 0; index < (int)rows.size(); ++index)
      if (rows[index] == row) return index;
   return -1;
}

bool Lists::page_has_row(Rml::Element *page)
{
   std::vector<Rml::Element*> rows;
   collect(page, "list-row", rows);
   for (auto *row : rows)
   {
      /* The page may be hidden, so we read only the local display of the row. */
      const auto *property = row->GetLocalProperty("display");
      if (!property || property->ToString() != "none") return true;
   }
   return false;
}

void Lists::mark_pager(Rml::Element *list, int page, int pages)
{
   struct Arrow { const char *name; bool dead; };
   const Arrow arrows[] = {{"list-pager-prev", page <= 0},
                           {"list-pager-next", page >= pages - 1}};
   for (const auto& arrow : arrows)
   {
      std::vector<Rml::Element*> found;
      collect(list, arrow.name, found);
      for (auto *element : found) element->SetClass("disabled", arrow.dead);
   }
}

int Lists::turn_list_page(int delta) const
{
   auto *list = visible_list();
   if (!list) return -1;
   std::vector<Rml::Element*> pages;
   collect(list, "list-page", pages);
   std::vector<Rml::Element*> usable;
   for (auto *page : pages)
      if (page_has_row(page)) usable.push_back(page);
   if (usable.size() < 2) return -1;
   int current = 0;
   for (size_t index = 0; index < usable.size(); ++index)
      if (!display_none(usable[index])) current = (int)index;
   const int next = current + (delta < 0 ? -1 : 1);
   if (next < 0 || next >= (int)usable.size()) return -1;
   for (size_t index = 0; index < usable.size(); ++index)
      if ((int)index == next) usable[index]->RemoveProperty("display");
      else usable[index]->SetProperty("display", "none");
   std::vector<Rml::Element*> counts;
   collect(list, "list-pager-count", counts);
   if (!counts.empty())
   {
      char label[32];
      std::snprintf(label, sizeof(label), "%d/%d", next + 1, (int)usable.size());
      counts[0]->SetInnerRML(label);
   }
   mark_pager(list, next, (int)usable.size());
   return next;
}

void Lists::select_in(Rml::Element *list, const char *row_id,
      const char *on, const char *off) const
{
   std::vector<Rml::Element*> rows;
   collect(list, "list-row", rows);
   for (auto *row : rows)
   {
      const bool selected = row_id && row->GetId() == row_id;
      row->SetClass("selected", selected);
      if (auto *state = document.root()->GetElementById(row->GetId() + "-state"))
         state->SetInnerRML(Rml::StringUtilities::EncodeRml(
               selected ? (on ? on : "") : (off ? off : "")));
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

void Lists::set_text(const std::string& id, const char *text) const
{
   if (!document.root()) return;
   if (auto *element = document.root()->GetElementById(id))
      element->SetInnerRML(Rml::StringUtilities::EncodeRml(text ? text : ""));
}

void Lists::set_row_text(const char *id, const char *title,
      const char *detail, const char *state) const
{
   if (!id || !*id) return;
   const std::string row(id);
   set_text(row + "-title", title);
   set_text(row + "-detail", detail);
   set_text(row + "-state", state);
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
   const std::string title_id = std::string(id) + "-title";
   const std::string source = text ? text : "";
   auto *element = document.root() ? document.root()->GetElementById(title_id) : nullptr;
   if (!element || !document.get_context())
   {
      set_text(title_id, source.c_str());
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
      set_text(title_id, source.c_str());
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
   set_text(title_id, (slice(source, 0, head) + mark
         + slice(source, total - tail, tail)).c_str());
}

Rml::Element *Lists::list_element(const char *list_id) const
{
   if (!document.root() || !list_id || !*list_id) return nullptr;
   return document.root()->GetElementById(list_id);
}

int Lists::rows_in(const char *list_id) const
{
   std::vector<Rml::Element*> rows;
   collect(list_element(list_id), "list-row", rows);
   return (int)rows.size();
}

const char *Lists::row_in(const char *list_id, int index)
{
   std::vector<Rml::Element*> rows;
   collect(list_element(list_id), "list-row", rows);
   row_in_buffer.clear();
   if (index >= 0 && index < (int)rows.size()) row_in_buffer = rows[index]->GetId();
   return row_in_buffer.c_str();
}

void Lists::retarget_pages(const char *list_id) const
{
   auto *list = list_element(list_id);
   if (!list) return;
   std::vector<Rml::Element*> pages;
   collect(list, "list-page", pages);
   std::vector<Rml::Element*> usable;
   for (auto *page : pages)
      if (page_has_row(page)) usable.push_back(page);
      else page->SetProperty("display", "none");
   for (size_t index = 0; index < usable.size(); ++index)
      if (index == 0) usable[index]->RemoveProperty("display");
      else usable[index]->SetProperty("display", "none");
   std::vector<Rml::Element*> pagers;
   collect(list, "list-pager", pagers);
   if (pagers.empty()) return;
   if (usable.size() < 2)
   {
      pagers[0]->SetProperty("display", "none");
      return;
   }
   pagers[0]->RemoveProperty("display");
   std::vector<Rml::Element*> counts;
   collect(list, "list-pager-count", counts);
   if (!counts.empty())
   {
      char label[32];
      std::snprintf(label, sizeof(label), "1/%d", (int)usable.size());
      counts[0]->SetInnerRML(label);
   }
   mark_pager(list, 0, (int)usable.size());
}

void Lists::place_list(const char *list_id, const char *anchor_id, int width_dp) const
{
   place_binds_popup(document.root(), document.get_context(),
         list_id, anchor_id, width_dp);
}
}
