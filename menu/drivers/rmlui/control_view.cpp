#include "document_contract.hpp"
#include "control_view.hpp"
#include "document.hpp"
#include "listeners.hpp"
#include "elements.hpp"
#include <RmlUi/Core/StringUtilities.h>
#include <cstring>
#include <vector>
namespace rib {
void ControlView::wire_controls(const rib_controls_catalog& catalog)
{
   if (!document.root())
      return;
   /* Walk the elements in the document, not a list of ids.
    *
    * We generate the scene markup from the console package, so the elements
    * in the document are the declared controls, however many there are, and
    * their names are not in this code. */
   std::vector<std::string> wired_groups;
   for (int index = 0; index < catalog.count; ++index)
   {
      const char *control_id = catalog.entries[index].id;
      if (!control_id || !*control_id)
         break;
      const rib::Event action{RIB_RMLUI_ACTION_CONTROL, control_id};
      const std::string ids[] = {
         document_contract::ControlPrefix + std::string(control_id),
         document_contract::ControlHitPrefix + std::string(control_id)
      };
      for (const std::string& id : ids)
         if (Rml::Element *element = document.root()->GetElementById(id))
         {
            element->AddEventListener(Rml::EventId::Click,
                  new ActionListener(events, action));
            element->AddEventListener(Rml::EventId::Mouseover,
                  new HoverListener(hovered, action));
            element->AddEventListener(Rml::EventId::Mouseout,
                  new HoverListener(hovered, action));
         }
      const char *group = catalog.entries[index].group;
      if (!group || !*group)
         continue;
      const std::string name(group);
      bool seen = false;
      for (const std::string& wired : wired_groups)
         if (wired == name)
            seen = true;
      if (seen)
         continue;
      wired_groups.push_back(name);
      if (Rml::Element *element = document.root()->GetElementById(document_contract::ControlGroupPrefix + name))
      {
         element->AddEventListener(Rml::EventId::Click,
               new ActionListener(events, action));
         element->AddEventListener(Rml::EventId::Mouseover,
               new HoverListener(hovered, action));
         element->AddEventListener(Rml::EventId::Mouseout,
               new HoverListener(hovered, action));
      }
   }

}

void ControlView::wire_device_picker(const rib_controls_catalog& catalog)
{
   if (!document.root())
      return;
   if (Rml::Element *current = document.root()->GetElementById(document_contract::ControlsDeviceCurrent))
      current->AddEventListener(Rml::EventId::Click,
            new ActionListener(events, RIB_RMLUI_ACTION_DEVICE_PICKER_TOGGLE));

   for (int index = 0; index < catalog.device_count; ++index)
   {
      const char *id = catalog.devices[index].id;
      if (!id || !*id)
         continue;
      if (Rml::Element *option =
            document.root()->GetElementById(document_contract::ControlsDeviceOptionPrefix + std::string(id)))
         option->AddEventListener(Rml::EventId::Click,
               new ActionListener(events, {RIB_RMLUI_ACTION_DEVICE_PICKER_CHOOSE, id}, false));
   }
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

bool ControlView::set_scene(const char *markup)
{
   if (!document.root() || !markup)
      return false;
   Rml::Element *scene = document.root()->GetElementById(document_contract::ControllerScene);
   if (!scene)
      return false;
   scene->SetInnerRML(markup);
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
