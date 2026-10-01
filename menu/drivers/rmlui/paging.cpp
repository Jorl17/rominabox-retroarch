#include "paging.hpp"

#include "document_contract.hpp"
#include "elements.hpp"
#include "words.hpp"

#include <algorithm>
#include <string>
#include <utility>

namespace rib {
namespace paging {
namespace {
bool row_shown(Rml::Element *row)
{
   /* The page may be hidden, so we read only the display of the row, set on
    * it or by the rules of the design. For example, we disable a switch with
    * no effect in the game, and the design sets whether it is hidden. Update
    * the styles before calling. */
   const auto *property = row->GetLocalProperty("display");
   return !property || property->ToString() != "none";
}

std::vector<Rml::Element*> rows_on(Rml::Element *page)
{
   /* Everything on a page is a row. That is a row of the list, or an entry in
    * the design that we page but that the player cannot choose. */
   std::vector<Rml::Element*> rows;
   for (int index = 0; page && index < page->GetNumChildren(); ++index)
      rows.push_back(page->GetChild(index));
   return rows;
}

bool page_has_row(Rml::Element *page)
{
   const auto rows = rows_on(page);
   return std::any_of(rows.begin(), rows.end(), row_shown);
}

std::vector<Rml::Element*> pages_of(Rml::Element *list)
{
   std::vector<Rml::Element*> pages;
   collect(list, document_contract::ListPage, pages);
   return pages;
}

/* Update the styles, so that we read a row the design hides as hidden. */
void refresh(Rml::Element *list)
{
   if (auto *context = list->GetContext())
      context->Update();
}

void mark_pager(Rml::Element *list, int page, int pages)
{
   struct Arrow { const char *name; bool dead; };
   const Arrow arrows[] = {{document_contract::ListPagerPrev, page <= 0},
                           {document_contract::ListPagerNext, page >= pages - 1}};
   for (const auto& arrow : arrows)
   {
      std::vector<Rml::Element*> found;
      collect(list, arrow.name, found);
      for (auto *element : found) element->SetClass(document_contract::Disabled, arrow.dead);
   }
}

void show_page(Rml::Element *list, const std::vector<Rml::Element*>& pages, int page)
{
   for (size_t index = 0; index < pages.size(); ++index)
      show(pages[index], (int)index == page);
   if (Rml::Element *count = find_class(list, document_contract::ListPagerCount))
      write_text(count, say(Word::PageCount, {{"page", std::to_string(page + 1)},
            {"pages", std::to_string(pages.size())}}));
   mark_pager(list, page, (int)pages.size());
}

std::vector<Rml::Element*> usable_pages(Rml::Element *list)
{
   std::vector<Rml::Element*> usable;
   for (auto *page : pages_of(list))
      if (page_has_row(page)) usable.push_back(page);
   return usable;
}
}

std::vector<Rml::Element*> lists_in(Rml::Element *scope)
{
   /* The lists with a page size from the exporter. A design may also give the
    * class list to the box around a list, to place it on the screen, and that
    * box has no pages. */
   std::vector<Rml::Element*> lists, paged;
   collect(scope, document_contract::List, lists);
   for (auto *list : lists)
      if (list->HasAttribute(document_contract::PageSizeAttribute))
         paged.push_back(list);
   return paged;
}

Rml::Element *list_of(Rml::Element *element)
{
   for (auto *at = element; at; at = at->GetParentNode())
      if (at->IsClassSet(document_contract::List)
            && at->HasAttribute(document_contract::PageSizeAttribute))
         return at;
   return nullptr;
}

Rml::Element *page_of(Rml::Element *element)
{
   for (auto *at = element; at; at = at->GetParentNode())
   {
      if (at->IsClassSet(document_contract::ListPage)) return at;
      if (at->IsClassSet(document_contract::List)) return nullptr;
   }
   return nullptr;
}

std::vector<Rml::Element*> rows(Rml::Element *list)
{
   std::vector<Rml::Element*> rows;
   for (auto *page : pages_of(list))
      for (auto *row : rows_on(page))
         rows.push_back(row);
   return rows;
}

int current(Rml::Element *list)
{
   const auto pages = pages_of(list);
   int current = 0;
   for (size_t index = 0; index < pages.size(); ++index)
      if (!display_none(pages[index])) current = (int)index;
   return current;
}

Rml::Element *shown(Rml::Element *list)
{
   for (auto *page : pages_of(list))
      if (!display_none(page)) return page;
   return nullptr;
}

Rml::Element *add_page(Rml::Element *list)
{
   auto page = list->GetOwnerDocument()->CreateElement("div");
   page->SetClass(document_contract::ListPage, true);
   return list->InsertBefore(std::move(page), find_class(list, document_contract::ListPager));
}

/* Split a list into pages, keeping the rows in document order. We put the
 * visible rows on pages of the page size of the list, and a hidden row on
 * the page of the visible row before it, so it is in its place when we show
 * it. We do not move rows already in place, so focus and hover stay on
 * them. */
void split(Rml::Element *list, const std::vector<Rml::Element*>& rows, int page,
      Rml::Element *keep)
{
   if (!list || !list->GetOwnerDocument()) return;
   refresh(list);
   const int size = std::max(1, list->GetAttribute<int>(document_contract::PageSizeAttribute, 1));
   std::vector<int> page_for(rows.size());
   int visible = 0;
   for (size_t index = 0; index < rows.size(); ++index)
   {
      const bool on = row_shown(rows[index]);
      page_for[index] = (on ? visible : std::max(visible - 1, 0)) / size;
      visible += on ? 1 : 0;
   }
   const int page_count = (visible + size - 1) / size;
   const int kept = std::max(page_count, (int)rows.size() > visible ? 1 : 0);
   auto pages = pages_of(list);
   while ((int)pages.size() < kept)
      pages.push_back(add_page(list));
   std::vector<int> filled(pages.size(), 0);
   for (size_t index = 0; index < rows.size(); ++index)
   {
      const int on = page_for[index];
      Rml::Element *at = pages[on]->GetChild(filled[on]++);
      if (rows[index] == at) continue;
      auto moved = rows[index]->GetParentNode()->RemoveChild(rows[index]);
      pages[on]->InsertBefore(std::move(moved), at);
   }
   for (size_t index = kept; index < pages.size(); ++index)
      list->RemoveChild(pages[index]);
   pages.resize(kept);
   for (int index = 0; keep && index < page_count; ++index)
      if (pages[index]->Contains(keep)) page = index;
   if (page_count > 0)
      show_page(list, {pages.begin(), pages.begin() + page_count},
            std::max(0, std::min(page, page_count - 1)));
   else if (kept > 0)
      show(pages[0], false);
   show(find_class(list, document_contract::ListPager), page_count > 1);
}

void split_all(Rml::Element *scope)
{
   for (auto *list : lists_in(scope))
      split(list, rows(list), 0);
}

void resplit(Rml::Element *scope, Rml::Element *keep)
{
   for (auto *list : lists_in(scope))
      split(list, rows(list), current(list), keep);
}

int turn(Rml::Element *list, int delta)
{
   if (!list) return -1;
   refresh(list);
   const auto usable = usable_pages(list);
   if (usable.size() < 2) return -1;
   int at = 0;
   for (size_t index = 0; index < usable.size(); ++index)
      if (!display_none(usable[index])) at = (int)index;
   const int next = at + (delta < 0 ? -1 : 1);
   if (next < 0 || next >= (int)usable.size()) return -1;
   show_page(list, usable, next);
   return next;
}
}
}
