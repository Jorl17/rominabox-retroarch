#pragma once

#include <RmlUi/Core.h>
#include <map>
#include <string>

namespace rib {
class Document;
class EventQueue;

/* The slider and toggle parts of the design, their listeners, and a pointer
 * drag. The elements belong to Document, so call clear_drag before teardown. */
class Parts
{
public:
   Parts(Document& document, EventQueue& events) : document(document), events(events) {}

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
   void end_drag();
   void clear_drag();
   void clear();
   const std::map<std::string, float>& fractions() const { return slider_fraction; }

private:
   static float clamp_fraction(float fraction);
   float fraction_at(Rml::Element *slider, int x) const;
   void paint_slider(Rml::Element *slider, float fraction, const char *readout);
   void remember_slider(const std::string& id, float fraction);
   static void note_slider_move(float before, float after);

   Document& document;
   EventQueue& events;
   std::map<std::string, float> slider_fraction;
   std::map<std::string, float> slider_step;
   Rml::Element *drag_element = nullptr;
   std::string drag_id;
   float drag_fraction = 0.0f;
   float drag_origin = 0.0f;
};
}
