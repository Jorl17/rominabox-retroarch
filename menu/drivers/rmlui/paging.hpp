#pragma once

#include <RmlUi/Core.h>
#include <vector>

namespace rib {
/* How we split a list into pages, with the same rules in the menu and in the
 * offscreen preview of its pictures. A paged list is a `list` with a page
 * size from the exporter, and everything on its pages is a row. Each function
 * takes only elements, and we reach the document and context through them. */
namespace paging {
/* The paged lists in `scope`. */
std::vector<Rml::Element*> lists_in(Rml::Element *scope);
/* The paged list that contains `element`, or nullptr. */
Rml::Element *list_of(Rml::Element *element);
/* The list page that contains `element`, or nullptr. A pager is on no page. */
Rml::Element *page_of(Rml::Element *element);
/* Every row on the pages of `list`, in order. The prototype row of a
 * generated list is on no page. */
std::vector<Rml::Element*> rows(Rml::Element *list);
/* The index of the current page of `list`. */
int current(Rml::Element *list);
/* The current page of `list`, or nullptr. */
Rml::Element *shown(Rml::Element *list);
/* A new empty page of `list`, before its pager. */
Rml::Element *add_page(Rml::Element *list);
/* Split `rows` in order into pages of the page size of `list`, and show page
 * `page`, or the one with `keep`. */
void split(Rml::Element *list, const std::vector<Rml::Element*>& rows, int page,
      Rml::Element *keep = nullptr);
/* Split every paged list in `scope` from its first page, as a document
 * loads. */
void split_all(Rml::Element *scope);
/* Split every paged list in `scope` again from its visible rows, and stay on
 * the current page, or show the one with `keep`. */
void resplit(Rml::Element *scope, Rml::Element *keep = nullptr);
/* Turn `list` by one page, back or on by `delta`. Returns the new page, or -1
 * when there is no page in that direction. */
int turn(Rml::Element *list, int delta);
}
}
