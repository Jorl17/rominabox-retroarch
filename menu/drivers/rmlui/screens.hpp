#pragma once

#include "events.h"
#include <string>
#include <vector>

namespace rib {
class Document;

/* The screens declared in the selected design, and their document actions. We
 * declare built-in screens after adding listeners, before design screens. */
class Screens
{
public:
   Screens(Document& document, EventQueue& events, Event& hovered)
      : document(document), events(events), hovered(hovered) {}

   const char *current() const { return active; }
   bool controls_visible() const;
   void remember(const char *id);
   void built_in_screens();
   void clear_screens();
   void declare_screen(const char *id, const char *panel,
         const char *heading, const char *footer, const char *button,
         const char *role = "");
   /* The declared role of screen `id`, or "" when it has none. */
   const char *role_of(const char *id) const;
   /* The first screen declared with `role`, or "" when there is none. */
   const char *with_role(const char *role) const;
   bool show_screen(const char *id);
   const char *screen_panel(const char *id) const;
   const char *pause_screen_button();
   void set_footer_hint(const char *hint) const;

private:
   struct Screen
   {
      std::string id, panel, heading, footer, button, role;
   };
   Document& document;
   EventQueue& events;
   Event& hovered;
   std::vector<Screen> screens;
   std::string pause_button_id;
   char active[32] = "pause";
};
}
