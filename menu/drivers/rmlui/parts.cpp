#include "document_contract.hpp"
#include "parts.hpp"

#include "document.hpp"
#include "elements.hpp"
#include "events.h"

#include <RmlUi/Core/StringUtilities.h>
#include <algorithm>
#include <utility>

namespace rib {
namespace {
Rml::Element *slider_ancestor(Rml::Element *node)
{
   for (; node; node = node->GetParentNode())
      if (node->IsClassSet(document_contract::Slider)) return node;
   return nullptr;
}

class PartToggleListener : public Rml::EventListener
{
public:
   PartToggleListener(EventQueue& events, std::string id)
      : events(events), id(std::move(id)) {}
   void ProcessEvent(Rml::Event& event) override
   {
      auto *element = event.GetCurrentElement();
      if (!element || element->HasAttribute("disabled")
            || element->IsClassSet(document_contract::Disabled))
         return;
      const bool on = !element->IsClassSet(document_contract::On);
      element->SetClass(document_contract::On, on);
      events.push({RIB_RMLUI_ACTION_PART_TOGGLE, id, 0.0f, on});
   }
   void OnDetach(Rml::Element*) override { delete this; }
private:
   EventQueue& events;
   std::string id;
};

class ArrowListener : public Rml::EventListener
{
public:
   ArrowListener(Parts& parts, std::string slider, int direction)
      : parts(parts), slider(std::move(slider)), direction(direction) {}
   void ProcessEvent(Rml::Event& event) override
   {
      auto *element = event.GetCurrentElement();
      if (!element || element->HasAttribute("disabled")
            || element->IsClassSet(document_contract::Disabled) || direction == 0)
         return;
      parts.nudge_slider(slider.c_str(), direction);
   }
   void OnDetach(Rml::Element*) override { delete this; }
private:
   Parts& parts;
   std::string slider;
   int direction;
};
}

void Parts::wire_part_toggles()
{
   walk(document.root(), [&](Rml::Element *element) {
      if ((element->IsClassSet(document_contract::Toggle)
               || element->IsClassSet(document_contract::Switch))
            && !element->GetId().empty())
         element->AddEventListener(Rml::EventId::Click,
               new PartToggleListener(events, element->GetId()));
      return Walk::Continue;
   });
}

void Parts::wire_arrows()
{
   walk(document.root(), [&](Rml::Element *element) {
      if (element->IsClassSet(document_contract::VolumeArrow))
      {
         const int direction = element->IsClassSet(document_contract::ArrowDown) ? -1
               : element->IsClassSet(document_contract::ArrowUp) ? 1 : 0;
         auto *slider = find_class(element->GetParentNode(), document_contract::Slider, true);
         if (slider && direction != 0)
            element->AddEventListener(Rml::EventId::Click,
                  new ArrowListener(*this, slider->GetId(), direction));
      }
      return Walk::Continue;
   });
}

float Parts::clamp_fraction(float fraction)
{
   if (fraction < 0.0f) return 0.0f;
   if (fraction > 1.0f) return 1.0f;
   return fraction;
}

float Parts::fraction_at(Rml::Element *slider, int x) const
{
   auto *track = find_class(slider, document_contract::SliderTrack);
   if (!track) return 0.0f;
   if (document.get_context()) document.get_context()->Update();
   const float left = track->GetAbsoluteOffset(Rml::BoxArea::Border).x;
   const float width = track->GetBox().GetSize(Rml::BoxArea::Border).x;
   if (width <= 1.0f) return 0.0f;
   return clamp_fraction((static_cast<float>(x) - left) / width);
}

void Parts::draw(Rml::Element *slider, float fraction, const char *readout)
{
   if (!slider) return;
   fraction = clamp_fraction(fraction);
   slider_fraction[slider->GetId()] = fraction;
   draw_slider(slider, fraction, slider_painted[slider->GetId()]);
   if (readout)
      write_text(find_class(slider, document_contract::SliderReadout), readout);
}

void Parts::paint_slider(Rml::Element *slider, float fraction)
{
   draw(slider, fraction, "");
}

void Parts::remember_slider(const std::string& id, float fraction)
{
   events.push({RIB_RMLUI_ACTION_SLIDER, id, clamp_fraction(fraction)});
}

void Parts::set_slider(const char *id, float fraction, const char *readout)
{
   if (!document.root() || !id) return;
   if (auto *slider = document.root()->GetElementById(id))
      if (slider->IsClassSet(document_contract::Slider))
         draw(slider, fraction, readout);
}

void Parts::set_slider_step(const char *id, float step)
{
   if (id && *id && step > 0.0f) slider_step[id] = step;
}

bool Parts::nudge_slider(const char *id, int direction)
{
   if (!id || direction == 0) return false;
   const auto step = slider_step.find(id);
   if (step == slider_step.end()) return false;
   float current = 0.0f;
   const auto found = slider_fraction.find(id);
   if (found != slider_fraction.end()) current = found->second;
   return commit_slider(id, current + (float)direction * step->second);
}

bool Parts::commit_slider(const char *id, float fraction)
{
   if (!document.root() || !id) return false;
   auto *slider = document.root()->GetElementById(id);
   if (!slider || !slider->IsClassSet(document_contract::Slider)) return false;
   draw(slider, fraction, nullptr);
   remember_slider(slider->GetId(), fraction);
   return true;
}

bool Parts::slider_drag(const char **id, float *fraction) const
{
   if (!drag_element) return false;
   if (id) *id = drag_id.c_str();
   if (fraction) *fraction = drag_fraction;
   return true;
}

bool Parts::part_is_slider(const char *id) const
{
   if (!document.root() || !id) return false;
   auto *element = document.root()->GetElementById(id);
   return element && element->IsClassSet(document_contract::Slider);
}

void Parts::begin_drag(Rml::Element *hovered, int x)
{
   auto *slider = slider_ancestor(hovered);
   if (!slider) return;
   drag_element = slider;
   drag_id = slider->GetId();
   slider->SetClass(document_contract::Dragging, true);
   drag_to(x);
}

void Parts::drag_to(int x)
{
   if (!drag_element) return;
   drag_fraction = fraction_at(drag_element, x);
   draw(drag_element, drag_fraction, nullptr);
}

Event Parts::end_drag()
{
   if (!drag_element) return {};
   drag_element->SetClass(document_contract::Dragging, false);
   drag_element = nullptr;
   return {RIB_RMLUI_ACTION_SLIDER, drag_id, clamp_fraction(drag_fraction)};
}

void Parts::forget()
{
   drag_element = nullptr;
   slider_painted.clear();
}

}
