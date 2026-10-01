#pragma once

#include <RmlUi/Core.h>
#include <string>
#include <vector>

namespace rib {
class Document;
class EventQueue;

/* Here we work on the list markup from the selected design. The elements we
 * return belong to Document, so do not keep them past a document reload. */
class Lists
{
public:
   Lists(Document& document, EventQueue& events) : document(document), events(events) {}

   void wire_lists(Rml::Element *scope = nullptr);
   struct Row
   {
      /* The state of the picture of the row. We show a placeholder while it
       * loads, and a mark when we could not fetch it. */
      enum class Badge { None, Loading, Ready };
      std::string id, title, detail, state, icon;
      bool selected = false;
      Badge badge = Badge::None;
      /* A row without a picture or second line. We mark it `line`, as we do
       * for such rows in the lists we write in the exporter. */
      bool line = false;
   };
   // Replace generated data through the staged prototype from the design, and
   // keep the current page where possible. We leave static lists unchanged.
   // We split lists into pages in paging.hpp.
   void replace_rows(const char *list_id, const std::vector<Row>& rows);
   /* For a switch, set the fact `on` and the word for its state in `<id>-state`. */
   void set_toggle(const char *id, const char *state, bool on);
   Rml::Element *visible_list() const;
   /* Turn `list`, or the visible list, by one page. Returns the page, or -1. */
   int turn_list_page(int delta, Rml::Element *list = nullptr) const;
   /* The current page of `list`, or of the visible list. */
   Rml::Element *shown_page(Rml::Element *list = nullptr) const;
   void mark_row(const char *id, const char *on, const char *off) const;
   void select_row(const char *list_id, const char *row_id,
         const char *on, const char *off) const;
   void set_row_text(const char *id, const char *title,
         const char *detail, const char *state) const;
   void fit_row_title(const char *id, const char *text) const;
   int rows_in(const char *list_id) const;
   /* The id of row `index` of the list, or "". */
   std::string row_in(const char *list_id, int index) const;
   /* Split the list again after we showed or hid rows, and show the page with
    * `keep_row`, or the first. */
   void retarget_pages(const char *list_id, const char *keep_row = nullptr) const;
   void place_list(const char *list_id, const char *anchor_id, int width_dp) const;

private:
   Rml::Element *list_element(const char *list_id) const;
   void select_in(Rml::Element *list, const char *row_id,
         const char *on, const char *off) const;
   float block_dp(Rml::Element *element) const;

   Document& document;
   EventQueue& events;
};
}
