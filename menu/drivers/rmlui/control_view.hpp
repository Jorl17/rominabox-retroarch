#pragma once
#include "declarations.h"
#include "events.h"
#include <string>
namespace rib {
class Document;
/* Controller scene, callouts and picker elements. The configuration is in
 * Controls, and in the view we read its catalog without a callback into it. */
class ControlView
{
public:
   ControlView(Document& document, EventQueue& events, Event& hovered)
      : document(document), events(events), hovered(hovered) {}
   /* Once per document. We put one listener on the scene and one on the
    * picker, and find the control or pad under the pointer when the event
    * arrives, so there is nothing to attach for a new scene, a new pad or
    * Reset. We borrow the catalog for as long as the document exists. */
   void wire(const rib_controls_catalog& catalog);
   void set_device_picker(const rib_controls_catalog& catalog, bool open, const char *chosen);
   /* Draw the scene for `profile`. scene_profile() is the pad we drew last
    * this way, empty while the document still has the exported scene. */
   bool set_scene(const char *profile, const char *markup);
   const char *scene_profile() const { return scene.c_str(); }
   /* The label and binding of a control, and whether `stop`, the element for
    * it in the scene, is waiting for input. */
   void set_control_state(const char *stop, const char *id, const char *label,
         const char *binding, bool capturing);
   /* We show CANCEL, marked capturing, only while we capture a binding. */
   void set_capturing(bool capturing);
private:
   Document& document;
   EventQueue& events;
   Event& hovered;
   std::string scene;
};
}
