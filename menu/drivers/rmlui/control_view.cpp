#include "document_contract.hpp"
#include "control_view.hpp"
#include "document.hpp"
#include "listeners.hpp"
#include "elements.hpp"
#include <RmlUi/Core/StringUtilities.h>
#include <cstring>
#include <vector>
namespace rib {
namespace {
/* The control an element in the scene stands for: the callout or the
 * stick's box it is part of. */
Event control_at(Rml::Element *element, Rml::Element *scene, const rib_controls_catalog& catalog)
{
   for (; element && element != scene; element = element->GetParentNode())
   {
      const std::string& id = element->GetId();
      if (id.empty())
         continue;
      for (int index = 0; index < catalog.count; ++index)
      {
         const rib_control_declaration& control = catalog.entries[index];
         if (id == document_contract::ControlPrefix + control.id
               || (!control.group.empty() && id == document_contract::ControlGroupPrefix + control.group))
         {
            if (element->HasAttribute("disabled") || element->IsClassSet(document_contract::Disabled))
               return {};
            return {RIB_RMLUI_ACTION_CONTROL, control.id};
         }
      }
   }
   return {};
}

class SceneListener : public Rml::EventListener
{
public:
   SceneListener(EventQueue& events, Event& hovered, const rib_controls_catalog& catalog)
      : events(events), hovered(hovered), catalog(catalog) {}
   void ProcessEvent(Rml::Event& event) override
   {
      const Event action = control_at(event.GetTargetElement(), event.GetCurrentElement(), catalog);
      if (action.kind == RIB_RMLUI_ACTION_NONE)
         return;
      if (event.GetId() == Rml::EventId::Click)
         events.push(action);
      else if (event.GetId() == Rml::EventId::Mouseover)
         hovered = action;
      else if (hovered.same_target(action))
         hovered = RIB_RMLUI_ACTION_NONE;
   }
   void OnDetach(Rml::Element*) override { delete this; }
private:
   EventQueue& events;
   Event& hovered;
   const rib_controls_catalog& catalog;
};

class PickerListener : public Rml::EventListener
{
public:
   explicit PickerListener(EventQueue& events) : events(events) {}
   void ProcessEvent(Rml::Event& event) override
   {
      const std::string option_prefix = document_contract::ControlsDeviceOptionPrefix;
      for (Rml::Element *element = event.GetTargetElement();
            element && element != event.GetCurrentElement(); element = element->GetParentNode())
      {
         const std::string& id = element->GetId();
         if (id == document_contract::ControlsDeviceCurrent)
         {
            if (!element->HasAttribute("disabled") && !element->IsClassSet(document_contract::Disabled))
               events.push(RIB_RMLUI_ACTION_DEVICE_PICKER_TOGGLE);
            return;
         }
         if (id.compare(0, option_prefix.size(), option_prefix) == 0)
         {
            events.push({RIB_RMLUI_ACTION_DEVICE_PICKER_CHOOSE, id.substr(option_prefix.size())});
            return;
         }
      }
   }
   void OnDetach(Rml::Element*) override { delete this; }
private:
   EventQueue& events;
};
}

void ControlView::wire(const rib_controls_catalog& catalog)
{
   scene.clear();
   if (!document.root())
      return;
   if (Rml::Element *element = document.root()->GetElementById(document_contract::ControllerScene))
   {
      // One listener per event. We delete each one when it is detached.
      for (const Rml::EventId id : {Rml::EventId::Click, Rml::EventId::Mouseover, Rml::EventId::Mouseout})
         element->AddEventListener(id, new SceneListener(events, hovered, catalog));
   }
   if (Rml::Element *picker = document.root()->GetElementById(document_contract::ControlsDevice))
      picker->AddEventListener(Rml::EventId::Click, new PickerListener(events));
}

void ControlView::set_device_picker(const rib_controls_catalog& catalog, bool open, const char *chosen)
{
   if (!document.root())
      return;
   document.set_shown(document_contract::ControlsDeviceList, open);
   for (int index = 0; index < catalog.device_count; ++index)
   {
      const std::string& id = catalog.devices[index].id;
      if (id.empty())
         continue;
      if (Rml::Element *option =
            document.root()->GetElementById(document_contract::ControlsDeviceOptionPrefix + id))
         option->SetClass(document_contract::Selected, chosen && id == chosen);
   }
   if (Rml::Element *current = document.root()->GetElementById(document_contract::ControlsDeviceCurrent))
      for (int index = 0; index < catalog.device_count; ++index)
         if (chosen && catalog.devices[index].id == chosen)
         {
            write_text(current, catalog.devices[index].name);
            break;
         }
}

bool ControlView::set_scene(const char *profile, const char *markup)
{
   if (!document.root() || !markup)
      return false;
   Rml::Element *element = document.root()->GetElementById(document_contract::ControllerScene);
   if (!element)
      return false;
   element->SetInnerRML(markup);
   scene = profile ? profile : "";
   return true;
}

void ControlView::set_control_state(const char *stop, const char *id,
      const char *label, const char *binding, bool capturing)
{
   if (!document.root() || !id)
      return;
   const std::string suffix(id);
   if (Rml::Element *control = stop ? document.root()->GetElementById(stop) : nullptr)
      control->SetClass(document_contract::Capturing, capturing);
   document.set_element_text((document_contract::ControlLabelPrefix + suffix).c_str(), label);
   document.set_element_text((document_contract::ControlBindingPrefix + suffix).c_str(), binding);
}

void ControlView::set_capturing(bool capturing)
{
   if (!document.root())
      return;
   document.set_class(document_contract::ControlsCancel, document_contract::Capturing, capturing);
   document.set_shown(document_contract::ControlsCancel, capturing);
}
}
