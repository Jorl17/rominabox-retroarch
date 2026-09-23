#include "screens.hpp"

#include "document.hpp"
#include <RmlUi/Core/StringUtilities.h>
#include <utility>
#include <cstdio>
#include <cstring>

namespace rib {
std::set<std::string> Screens::wired_screen_buttons;

namespace {
class ScreenListener : public Rml::EventListener
{
public:
   ScreenListener(EventQueue& events, std::string id)
      : events(events), id(std::move(id)) {}
   void ProcessEvent(Rml::Event&) override
   {
      events.push({RIB_RMLUI_ACTION_SHOW_SCREEN, id});
   }
   void OnDetach(Rml::Element*) override { delete this; }
private:
   EventQueue& events;
   std::string id;
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
   declare_screen("pause", "pause-panel", "GAME PAUSED",
         "ESC  CONTINUE", "controls-back");
   declare_screen("controls", "controls-panel", "CONTROLS",
         "ESC  BACK", "controls");
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
   /* We load the document before we read a design. We keep the set of
    * elements with listeners for the whole process, across declarations and
    * new documents. */
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
            if (wired_screen_buttons.insert(one).second)
               if (auto *element = document.root()->GetElementById(one))
                  element->AddEventListener(Rml::EventId::Click,
                        new ScreenListener(events, id));
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
   if (auto *heading = document.root()->GetElementById("heading"))
      heading->SetInnerRML(Rml::StringUtilities::EncodeRml(wanted->heading));
   if (!wanted->footer.empty())
      if (auto *footer = document.root()->GetElementById("footer-hint"))
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
   if (auto *footer = document.root()->GetElementById("footer-hint"))
      footer->SetInnerRML(Rml::StringUtilities::EncodeRml(hint ? hint : ""));
}
}
