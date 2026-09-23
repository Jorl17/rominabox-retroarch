#pragma once
#include "events.h"
#include "menu_api.h"

namespace rib {
class Focus;
class Screens;
class Controls;
class Slots;
/* The navigation rules for every region of the menu. We return immediate
 * intents to Menu to dispatch, and send clicks to the element listeners. */
class Navigation
{
public:
   Navigation(Focus& focus, Screens& screens, Controls& controls, Slots& slots)
      : focus(focus), screens(screens), controls(controls), slots(slots) {}
   Event key(rib_key action);
   void focus_list(int index);
   void paint_list();
   void focus_pause(Event event, bool direction_up);
private:
   Event part_key(rib_key action);
   Event list_key(rib_key action, int rows);
   Event controls_key(rib_key action);
   Event pause_key(rib_key action);
   Event back();
   int pause_row(char ids[][64], int capacity);
   int pause_row_index(const char ids[][64], int count);
   void focus_pause_row(int index, bool direction_up);
   Focus& focus;
   Screens& screens;
   Controls& controls;
   Slots& slots;
};
}
