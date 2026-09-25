#pragma once

#include "declarations.h"
#include "events.h"
#include "screen_role.hpp"
#include <string>
#include <vector>

namespace rib {
class Document;

/* The screens declared in the selected design, and their document actions.
 * In the menu we find the screens that we treat specially by their declared
 * role, never by an id. */
class Screens
{
public:
   Screens(Document& document, EventQueue& events, Event& hovered)
      : document(document), events(events), hovered(hovered) {}

   /* The screen showing now, by its declared id. */
   const std::string& current() const { return active; }
   ScreenRole current_role() const { return role_of(active); }
   bool showing(ScreenRole role) const
   {
      return role != ScreenRole::None && current_role() == role;
   }
   /* Take `id` as the screen showing, without showing it. */
   void remember(const std::string& id) { active = id; }
   void clear_screens();
   void declare_screen(const ScreenDeclaration& screen);
   /* The declared role of screen `id`, None when it has none. */
   ScreenRole role_of(const std::string& id) const;
   /* The first screen declared with `role`, or "" when there is none. */
   const std::string& with_role(ScreenRole role) const;
   bool show_screen(const std::string& id);
   /* The panel of screen `id`, or "". */
   const std::string& screen_panel(const std::string& id) const;
   void set_footer_hint(const char *hint) const;
   /* The footer declared for the screen showing now. */
   void restore_footer() const;

private:
   const ScreenDeclaration *find(const std::string& id) const;
   Document& document;
   EventQueue& events;
   Event& hovered;
   std::vector<ScreenDeclaration> screens;
   std::string active;
};
}
