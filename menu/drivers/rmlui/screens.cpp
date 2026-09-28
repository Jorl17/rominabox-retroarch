#include "document_contract.hpp"
#include "screens.hpp"

#include "document.hpp"
#include "elements.hpp"
#include <utility>

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
            button->GetAttribute<std::string>(document_contract::ScreenTargetAttribute, "")});
   }
   void OnDetach(Rml::Element*) override { delete this; }
private:
   EventQueue& events;
};

const std::string nothing;
}

void Screens::clear_screens()
{
   screens.clear();
}

void Screens::declare_screen(const ScreenDeclaration& screen)
{
   if (screen.id.empty() || screen.panel.empty())
      return;
   screens.push_back(screen);
   /* We keep the listener and its target on the element. A new document has
    * new elements, and for a repeated declaration we update the target without
    * adding a listener. We keep no registry for longer than its document. */
   if (!document.root())
      return;
   for (const std::string& button : screen.buttons)
      if (auto *element = document.root()->GetElementById(button))
      {
         if (!element->HasAttribute(document_contract::ScreenTargetAttribute))
            element->AddEventListener(Rml::EventId::Click, new ScreenListener(events));
         element->SetAttribute(document_contract::ScreenTargetAttribute, screen.id);
      }
}

const ScreenDeclaration *Screens::find(const std::string& id) const
{
   for (const ScreenDeclaration& screen : screens)
      if (screen.id == id)
         return &screen;
   return nullptr;
}

bool Screens::show_screen(const std::string& id)
{
   const ScreenDeclaration *wanted = document.root() ? find(id) : nullptr;
   if (!wanted)
      return false;
   events.clear();
   hovered = RIB_RMLUI_ACTION_NONE;
   for (const ScreenDeclaration& screen : screens)
      show(document.root()->GetElementById(screen.panel), &screen == wanted);
   document.set_element_text(document_contract::Heading, wanted->heading.c_str());
   if (!wanted->footer.empty())
      document.set_hint(document_contract::FooterHint, wanted->footer.c_str());
   return true;
}

ScreenRole Screens::role_of(const std::string& id) const
{
   const ScreenDeclaration *screen = find(id);
   return screen ? screen->role : ScreenRole::None;
}

const std::string& Screens::with_role(ScreenRole role) const
{
   if (role != ScreenRole::None)
      for (const ScreenDeclaration& screen : screens)
         if (screen.role == role)
            return screen.id;
   return nothing;
}

const std::string& Screens::screen_panel(const std::string& id) const
{
   const ScreenDeclaration *screen = find(id);
   return screen ? screen->panel : nothing;
}

void Screens::set_footer_hint(const char *hint) const
{
   document.set_hint(document_contract::FooterHint, hint ? hint : "");
}

void Screens::restore_footer() const
{
   const ScreenDeclaration *screen = find(active);
   if (screen && !screen->footer.empty())
      set_footer_hint(screen->footer.c_str());
}
}
