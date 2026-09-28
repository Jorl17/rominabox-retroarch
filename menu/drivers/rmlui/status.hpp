#pragma once

#include <RmlUi/Core/Element.h>
#include <string>

namespace rib {
class Document;

/* Set the text of a status line. When `text` is empty, put back the text
 * the line had in the design, which we save in its data-prompt attribute
 * the first time. */
void paint_status_line(Rml::Element *line, const std::string& text);

/* The main status line and the lines on the CONTROLS and HOTKEYS screens. We
 * show a message for five seconds, then put back the text from the design. */
class Status
{
public:
   explicit Status(Document& document) : document(document) {}
   void set_main(const char *text);
   void set_controls(const char *text);
   void set_menu_controls(const char *text);
   void expire();
   const std::string& main_text() const { return main.text; }

private:
   struct Message { std::string text; double expires = 0; };
   void show(Message& message, const char *id, const char *text);
   void expire(Message& message, const char *id);
   Document& document;
   Message main, controls, menu_controls;
};
}
