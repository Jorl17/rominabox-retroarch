#pragma once

#include "events.h"
#include "setting_display.hpp"
#include <RmlUi/Core.h>
#include <map>
#include <string>

namespace rib {
class Document;

/* The slider and toggle parts of the design, their listeners, and a pointer
 * drag. The elements belong to Document, and on shutdown we clear the drag.
 * We draw the slider of a setting here, so we keep its position for the keys
 * and the pointer that move it. */
class Parts : public SliderPainter
{
public:
   /* Put the slider of a setting at `fraction` and write its readout. */
   void paint_slider(Rml::Element *slider, float fraction, const std::string& readout) override;
   Parts(Document& document, EventQueue& events) : document(document), events(events) {}

   /* The toggle part, and any other element we mark `switch` in composition,
    * such as a player setting drawn as an Options entry. */
   void wire_part_toggles();
   void wire_arrows();
   void set_slider(const char *id, float fraction, const char *readout);
   void set_slider_step(const char *id, float step);
   bool nudge_slider(const char *id, int direction);
   bool commit_slider(const char *id, float fraction);
   bool slider_drag(const char **id, float *fraction) const;
   bool part_is_slider(const char *id) const;
   void begin_drag(Rml::Element *hovered, int x);
   void drag_to(int x);
   /* End a drag. Returns the final position of the slider, which stays there,
    * or nothing when there was no drag. */
   Event end_drag();
   /* We are closing the document, so forget all its elements and drawings. */
   void forget();
   const std::map<std::string, float>& fractions() const { return slider_fraction; }

private:
   static float clamp_fraction(float fraction);
   float fraction_at(Rml::Element *slider, int x) const;
   void draw(Rml::Element *slider, float fraction, const char *readout);
   void remember_slider(const std::string& id, float fraction);

   Document& document;
   EventQueue& events;
   std::map<std::string, float> slider_fraction;
   std::map<std::string, float> slider_step;
   /* The last values we set for the fill and thumb of each slider. */
   std::map<std::string, SliderPainted> slider_painted;
   Rml::Element *drag_element = nullptr;
   std::string drag_id;
   float drag_fraction = 0.0f;
};
}
