#pragma once
#include "document.hpp"
#include "events.h"
#include "focus.hpp"
#include "lists.hpp"
#include "parts.hpp"
#include "status.hpp"
#include "slots.hpp"
#include "screens.hpp"
#include "control_view.hpp"
namespace rib {
/* Composition of the menu document and its views. We keep the presentation
 * caches for the whole process, and at shutdown we release only document and
 * input state. We pass each feature the components it uses, never this container. */
class View
{
public:
   bool initialize(const char *assets, const std::vector<std::string>& fonts,
         int width, int height, bool core_context, const rib_controls_catalog& controls);

   void shutdown();
   void render(int width, int height);
   void clear_intents();
   void set_overlay_mode(bool only_overlays);
   void pointer_move(int x, int y);
   void pointer_button(bool down);
   /* The pointer has gone, so release what it was pressing. Returns the end
    * of a drag cut short, for the caller to handle or drop. */
   Event pointer_leave();
   bool move_pointer_to(const char *id);
   /* Once a frame, after every pointer move. When the pointer moved onto a
    * stop, focus it without a sound. When it did not move, the focus stays
    * where the keys put it. Never move focus into or out of a text field. */
   void follow_pointer();
   Document document;
   EventQueue intents;
   Event hovered;
   Focus focus;
   Lists lists{document, intents};
   Parts parts{document, intents};
   Status status{document};
   Slots slots{document, focus, status};
   Screens screens{document, intents, hovered};
   ControlView controls{document, intents, hovered};
private:
   void wire_document();
   const rib_controls_catalog *catalog = nullptr;
   bool pointer_down = false;
   int pointer_x = 0, pointer_y = 0;
   /* Its position at the last follow_pointer, unknown until the first frame. */
   bool pointer_settled = false;
   int settled_x = 0, settled_y = 0;
};
/* The one retained presentation in the player. We compose it here and bind it
 * in Menu. A document-only test may create a separate View. */
View& menu_view();
}
