#pragma once
#include <RmlUi/Core.h>
#include <RmlUi/Core/TextInputHandler.h>
#include <memory>

namespace rib {
class Document;
/* We attach platform composition to the RmlUi editor, never to achievement
 * state. Destroy the context before its handler. For a port, implement this
 * small boundary with the OS text service (or the existing RmlUi backend). */
class TextInputPlatform : public Rml::TextInputHandler
{
public:
   virtual void get_clipboard(Rml::String& text) const = 0;
   virtual void set_clipboard(const Rml::String& text) = 0;
   virtual void caret(Rml::Vector2f position, float line_height) = 0;
};
std::unique_ptr<TextInputPlatform> make_text_input_platform(Document& document);
}
