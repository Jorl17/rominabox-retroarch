#include "words.hpp"
#include "document_contract.hpp"
#include "screens.hpp"

#include "document.hpp"
#include "elements.hpp"
#include <RmlUi/Core/StringUtilities.h>
#include <utility>
#include <cstdio>
#include <cstring>

namespace rib {

namespace {
class ScreenListener : public Rml::EventListener
{
public:
   explicit ScreenListener(EventQueue& events) : events(events) {}
   void ProcessEvent(Rml::Event& event) override
   {
      Rml::Element *button = event.GetCurrentElement();
      if (button->HasAttribute("disabled") || button->IsClassSet(document_contract::Disabled))
         return;
      /* On return we focus the button the player opened the screen with. A
       * pointer press already focuses it, but a script click does not. */
      button->Focus(true);
      events.push({RIB_RMLUI_ACTION_SHOW_SCREEN,
            button->GetAttribute<std::string>("data-screen-target", "")});
   }
   void OnDetach(Rml::Element*) override { delete this; }
private:
   EventQueue& events;
};
}

bool Screens::controls_visible() const
{
   return std::strcmp(active, "controls") == 0;
}

void Screens::remember(const char *id)
{
   std::snprintf(active, sizeof(active), "%s", id ? id : "");
}

void Screens::built_in_screens()
{
   /* Declare it and add its listeners once, as for a design screen. BACK on
    * Controls is not the button of Pause. With it the player goes back to the
    * screen they came from (we add its listener in view.cpp). */
   declare_screen("pause", document_contract::PausePanel, rib::words::PausedHeading,
         rib::words::ContinueHint, "");
   declare_screen("controls", document_contract::ControlsPanel, rib::words::ControlsHeading,
         rib::words::BackHint, document_contract::Controls);
}

void Screens::clear_screens()
{
   screens.clear();
}

void Screens::declare_screen(const char *id, const char *panel,
      const char *heading, const char *footer, const char *button,
      ScreenRole role)
{
   if (!id || !*id || !panel || !*panel)
      return;
   screens.push_back(Screen{id, panel, heading ? heading : "",
         footer ? footer : "", button ? button : "", role});
   /* We keep the listener and its target on the element. A new document has
    * new elements, and for a repeated declaration we update the target without
    * adding a listener. We keep no registry for longer than its document. */
   if (document.root() && button && *button)
   {
      const char *cursor = button;
      while (*cursor)
      {
         while (*cursor == ' ')
            ++cursor;
         const char *end = cursor;
         while (*end && *end != ' ')
            ++end;
         if (end > cursor)
         {
            const std::string one(cursor, end);
            if (auto *element = document.root()->GetElementById(one))
            {
               if (!element->HasAttribute("data-screen-target"))
                  element->AddEventListener(Rml::EventId::Click, new ScreenListener(events));
               element->SetAttribute("data-screen-target", std::string(id));
            }
         }
         cursor = end;
      }
   }
}

bool Screens::show_screen(const char *id)
{
   if (!document.root() || !id || !*id)
      return false;
   const Screen *wanted = nullptr;
   for (const Screen& screen : screens)
      if (screen.id == id)
      {
         wanted = &screen;
         break;
      }
   if (!wanted)
      return false;

   events.clear();
   hovered = RIB_RMLUI_ACTION_NONE;
   for (const Screen& screen : screens)
      show(document.root()->GetElementById(screen.panel), &screen == wanted);
   document.set_element_text(document_contract::Heading, wanted->heading.c_str());
   if (!wanted->footer.empty())
      document.set_element_text(document_contract::FooterHint, wanted->footer.c_str());
   return true;
}

ScreenRole Screens::role_of(const char *id) const
{
   if (id)
      for (const Screen& screen : screens)
         if (screen.id == id)
            return screen.role;
   return ScreenRole::None;
}

const char *Screens::with_role(ScreenRole role) const
{
   if (role != ScreenRole::None)
      for (const Screen& screen : screens)
         if (screen.role == role)
            return screen.id.c_str();
   return "";
}

const char *Screens::screen_panel(const char *id) const
{
   if (!id)
      return "";
   for (const Screen& screen : screens)
      if (screen.id == id)
         return screen.panel.c_str();
   return "";
}

const char *Screens::pause_screen_button()
{
   pause_button_id.clear();
   if (!document.root() || screens.empty())
      return pause_button_id.c_str();
   auto *pause = document.root()->GetElementById(screens.front().panel);
   if (!pause)
      return pause_button_id.c_str();
   for (const Screen& screen : screens)
   {
      const char *cursor = screen.button.c_str();
      while (*cursor)
      {
         while (*cursor == ' ')
            ++cursor;
         const char *end = cursor;
         while (*end && *end != ' ')
            ++end;
         if (end > cursor)
         {
            const std::string one(cursor, end);
            for (auto *element = document.root()->GetElementById(one); element;
                  element = element->GetParentNode())
               if (element == pause)
               {
                  pause_button_id = one;
                  return pause_button_id.c_str();
               }
         }
         cursor = end;
      }
   }
   return pause_button_id.c_str();
}

void Screens::set_footer_hint(const char *hint) const
{
   document.set_element_text(document_contract::FooterHint, hint ? hint : "");
}
}
