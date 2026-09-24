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
/* The control an element in the scene stands for: its callout, its picture
 * button, or the group it belongs to. */
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
         if (id == document_contract::ControlPrefix + std::string(control.id)
               || id == document_contract::ControlHitPrefix + std::string(control.id)
               || (control.group[0] && id == document_contract::ControlGroupPrefix + std::string(control.group)))
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
   if (Rml::Element *list = document.root()->GetElementById(document_contract::ControlsDeviceList))
   {
      if (open)
         list->RemoveProperty("display");
      else
         list->SetProperty("display", "none");
   }
   for (int index = 0; index < catalog.device_count; ++index)
   {
      const char *id = catalog.devices[index].id;
      if (!id || !*id)
         continue;
      if (Rml::Element *option =
            document.root()->GetElementById(document_contract::ControlsDeviceOptionPrefix + std::string(id)))
         option->SetClass(document_contract::Selected, chosen && !std::strcmp(chosen, id));
   }
   if (Rml::Element *current = document.root()->GetElementById(document_contract::ControlsDeviceCurrent))
      for (int index = 0; index < catalog.device_count; ++index)
         if (chosen && !std::strcmp(chosen, catalog.devices[index].id))
         {
            const char *name = catalog.devices[index].name;
            current->SetInnerRML(Rml::StringUtilities::EncodeRml(name ? name : chosen));
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

void ControlView::set_control_state(const char *id,
      const char *label, const char *binding, bool focused, bool capturing)
{
   if (!document.root() || !id)
      return;
   const std::string suffix(id);
   if (Rml::Element *control = document.root()->GetElementById(document_contract::ControlPrefix + suffix))
   {
      control->SetClass(document_contract::Focused, focused);
      control->SetClass(document_contract::Capturing, capturing);
   }
   if (Rml::Element *hit = document.root()->GetElementById(document_contract::ControlHitPrefix + suffix))
   {
      hit->SetClass(document_contract::Focused, focused);
      hit->SetClass(document_contract::Capturing, capturing);
   }
   if (Rml::Element *label_element =
         document.root()->GetElementById(document_contract::ControlLabelPrefix + suffix))
      label_element->SetInnerRML(Rml::StringUtilities::EncodeRml(
            label ? label : ""));
   if (Rml::Element *binding_element =
         document.root()->GetElementById(document_contract::ControlBindingPrefix + suffix))
      binding_element->SetInnerRML(Rml::StringUtilities::EncodeRml(
            binding ? binding : ""));
}

void ControlView::set_controls_action_focus(
      bool reset, bool back, bool cancel)
{
   if (!document.root())
      return;
   if (Rml::Element *element = document.root()->GetElementById(document_contract::ControlsReset))
      element->SetClass(document_contract::Focused, reset);
   if (Rml::Element *element = document.root()->GetElementById(document_contract::ControlsBack))
      element->SetClass(document_contract::Focused, back);
   if (Rml::Element *element = document.root()->GetElementById(document_contract::ControlsCancel))
   {
      element->SetClass(document_contract::Focused, cancel);
      if (cancel)
         element->RemoveProperty("display");
      else
         element->SetProperty("display", "none");
   }
}

void ControlView::focus_group(const char *group)
{
   if (!document.root())
      return;
   std::vector<Rml::Element*> groups;
   collect(document.root(), document_contract::ControlGroup, groups);
   const std::string wanted = group && *group
         ? std::string(document_contract::ControlGroupPrefix) + group : std::string();
   for (Rml::Element *element : groups)
      element->SetClass(document_contract::Focused, !wanted.empty() && element->GetId() == wanted);
}
}
