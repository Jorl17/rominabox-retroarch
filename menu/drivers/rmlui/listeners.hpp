#pragma once
#include "document_contract.hpp"
#include "events.h"
#include <RmlUi/Core.h>

namespace rib {
/* The element listeners point to the queue and hover target of the menu. In
 * RmlUi they are deleted with the document. Each event contains its payload. */
class ActionListener : public Rml::EventListener
{
public:
   ActionListener(EventQueue& events, Event action, bool check_disabled = true)
      : events(events), action(std::move(action)), check_disabled(check_disabled) {}
   void ProcessEvent(Rml::Event& event) override
   {
      if (check_disabled)
         if (auto *element = event.GetCurrentElement())
            if (element->HasAttribute("disabled") || element->IsClassSet(document_contract::Disabled)) return;
      events.push(action);
   }
   void OnDetach(Rml::Element*) override { delete this; }
private:
   EventQueue& events;
   Event action;
   bool check_disabled;
};
class HoverListener : public Rml::EventListener
{
public:
   HoverListener(Event& hovered, Event action) : hovered(hovered), action(std::move(action)) {}
   void ProcessEvent(Rml::Event& event) override
   {
      if (event.GetId() == Rml::EventId::Mouseout)
      {
         if (hovered.same_target(action)) hovered = RIB_RMLUI_ACTION_NONE;
         return;
      }
      hovered = action;
   }
   void OnDetach(Rml::Element*) override { delete this; }
private:
   Event& hovered;
   Event action;
};
}
