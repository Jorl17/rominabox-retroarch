#pragma once
#include "menu_api.h"
#include "document.hpp"
#include <RmlUi/Core/Elements/ElementFormControlInput.h>
#include <cstdint>
namespace rib {
/* We let RmlUi handle editing, selection and masking, and RetroArch the
 * controller keyboard. Here we show its keys in the selected design. */
class TextEntry : public Rml::EventListener
{
public:
   explicit TextEntry(Document& document) : document(document) {}
   void bind();
   void enable(const char *panel, const char *submit, const char *cancel);
   void disable();
   void update();
   /* A form is enabled and on screen, so the player types into it on the
    * physical keyboard, through physical(). */
   bool typing() const;
   bool physical(bool down, unsigned key, uint32_t character, uint16_t modifiers);
   bool controller(rib_key key);
   bool begin_native_input();
   bool keyboard_open() const;
   void cancel_keyboard();
   void ProcessEvent(Rml::Event& event) override;
private:
   static void complete(void *context, const char *value);
   Rml::ElementFormControlInput *input(const char *id) const;
   Document& document;
   std::string panel_id, submit_id, cancel_id, editing_id, original;
};
}
