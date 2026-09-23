#pragma once

#include <string>

namespace rib {
class Document;

/* The status lines of the menu. In Slots we explain a refused action only on
 * an empty main line, and we clear a line when its message expires. */
class Status
{
public:
   explicit Status(Document& document) : document(document) {}
   void set_main(const char *text);
   void set_controls(const char *text);
   void expire();
   const std::string& main_text() const { return main.text; }
   const std::string& controls_text() const { return controls.text; }

private:
   struct Message { std::string text; double expires = 0; };
   void show(Message& message, const char *id, const char *text);
   void expire(Message& message, const char *id);
   Document& document;
   Message main, controls;
};
}
