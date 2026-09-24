#include "words.hpp"
#include "document_contract.hpp"
#include "screens.hpp"

#include "document.hpp"
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
      events.push({RIB_RMLUI_ACTION_SHOW_SCREEN,
            event.GetCurrentElement()->GetAttribute<std::string>("data-screen-target", "")});
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
   /* Declare it and add its listeners once, as for a design screen. */
   declare_screen("pause", document_contract::PausePanel, rib::words::PausedHeading,
         rib::words::ContinueHint, document_contract::ControlsBack);
   declare_screen("controls", document_contract::ControlsPanel, rib::words::ControlsHeading,
         rib::words::BackHint, document_contract::Controls);
}

void Screens::clear_screens()
{
   screens.clear();
}

void Screens::declare_screen(const char *id, const char *panel,
      const char *heading, const char *footer, const char *button)
{
   if (!id || !*id || !panel || !*panel)
      return;
   screens.push_back(Screen{id, panel, heading ? heading : "",
         footer ? footer : "", button ? button : ""});
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
      if (auto *panel = document.root()->GetElementById(screen.panel))
      {
         if (&screen == wanted)
            panel->RemoveProperty("display");
         else
            panel->SetProperty("display", "none");
      }
   if (auto *heading = document.root()->GetElementById(document_contract::Heading))
      heading->SetInnerRML(Rml::StringUtilities::EncodeRml(wanted->heading));
   if (!wanted->footer.empty())
      if (auto *footer = document.root()->GetElementById(document_contract::FooterHint))
         footer->SetInnerRML(Rml::StringUtilities::EncodeRml(wanted->footer));
   return true;
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
   if (!document.root())
      return;
   if (auto *footer = document.root()->GetElementById(document_contract::FooterHint))
      footer->SetInnerRML(Rml::StringUtilities::EncodeRml(hint ? hint : ""));
}
}
