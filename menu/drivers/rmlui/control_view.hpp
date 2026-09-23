#pragma once
#include "declarations.h"
#include "events.h"
namespace rib {
class Document;
/* Controller scene, callouts and picker elements. The configuration is in
 * Controls, and we read its catalog here without a callback into Controls. */
class ControlView
{
public:
   ControlView(Document& document, EventQueue& events, Event& hovered)
      : document(document), events(events), hovered(hovered) {}
   void wire_controls(const rib_controls_catalog& catalog);
   void wire_device_picker(const rib_controls_catalog& catalog);
   void set_device_picker(const rib_controls_catalog& catalog, bool open, const char *chosen);
   bool set_scene(const char *markup);
   void set_control_state(const char *id, const char *label, const char *binding,
         bool focused, bool capturing);
   void set_controls_action_focus(bool reset, bool back, bool cancel);
   void focus_group(const char *group);
private:
   Document& document;
   EventQueue& events;
   Event& hovered;
};
}
