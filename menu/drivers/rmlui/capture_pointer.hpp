#pragma once

#include "events.h"

namespace rib {
/* Whether we take the pointer buttons as input for a capture, on either
 * screen with captures. Never the press that started it, nor one made to
 * leave (CANCEL or BACK). The pointer counts once the player has let go of
 * it. There is one for the menu, so both captures follow the same rule. */
class CapturePointer
{
public:
   /* A capture begins. The gesture that began it is not input for it. */
   void start() { ignored = true; }
   /* Each frame: whether the button is down, and whether it just went down
    * on the way out of a capture. */
   void frame(bool pressed, bool pressed_to_leave)
   {
      if (pressed_to_leave)
         ignored = true;
      else if (!pressed)
         ignored = false;
   }
   bool counts() const { return !ignored; }
   static bool leaves(int kind)
   {
      return kind == RIB_RMLUI_ACTION_CONTROLS_CANCEL || kind == RIB_RMLUI_ACTION_CONTROLS_BACK
            || kind == RIB_RMLUI_ACTION_HOTKEYS_CANCEL;
   }

private:
   bool ignored = false;
};
}
