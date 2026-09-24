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
      enum class Badge { None, Loading, Ready, Failed };
      std::string id, title, detail, state, icon;
      bool selected = false;
      Badge badge = Badge::None;
   };
   // Replace generated data through the staged prototype from the design, and
   // keep the current page where possible. We leave static lists unchanged.
   void replace_rows(const char *list_id, const std::vector<Row>& rows);
   void wire_toggles();
   void set_toggle(const char *id, const char *state, bool on);
   Rml::Element *visible_list() const;
   void visible_rows(std::vector<Rml::Element*> &rows) const;
   Rml::Element *visible_panel() const;
   void visible_controls(std::vector<Rml::Element*> &controls) const;
   int visible_row_count() const;
   /* Mark a row or a list control `focused` directly. We let RmlUi handle the
    * focus and mark it in Focus. We use these in test_rmlui_interaction.cpp to
    * measure a row with the class set in the styling checks. */
   void focus_list_row(int index) const;
   int list_control_count() const;
   const char *list_control_id(int index);
   void focus_list_control(int index) const;
   const char *list_row_id(int index);
   /* Turn `list`, or the visible list, by one page. Returns the page, or -1. */
   int turn_list_page(int delta, Rml::Element *list = nullptr) const;
   /* The first row on the current page of `list`, or of the visible list. */
   Rml::Element *first_row(Rml::Element *list = nullptr) const;
   void mark_row(const char *id, const char *on, const char *off) const;
   void select_row(const char *list_id, const char *row_id,
         const char *on, const char *off) const;
   void set_row_text(const char *id, const char *title,
         const char *detail, const char *state) const;
   void fit_row_title(const char *id, const char *text) const;
   int rows_in(const char *list_id) const;
   const char *row_in(const char *list_id, int index);
   void retarget_pages(const char *list_id) const;
   void place_list(const char *list_id, const char *anchor_id, int width_dp) const;

private:
   Rml::Element *list_element(const char *list_id) const;
   void set_text(const std::string& id, const char *text) const;
   void select_in(Rml::Element *list, const char *row_id,
         const char *on, const char *off) const;
   static bool page_has_row(Rml::Element *page);
   static void mark_pager(Rml::Element *list, int page, int pages);
   float block_dp(Rml::Element *element) const;

   Document& document;
   EventQueue& events;
   std::string control_id_buffer;
   std::string row_id_buffer;
   std::string row_in_buffer;
};
}
