#include "input/alt_enter_fullscreen.h"

static int return_down;
static int alt_held;
static int latched;

static int is_return(unsigned code)
{
   return code == ALT_ENTER_RETURN || code == ALT_ENTER_KP_RETURN;
}

void alt_enter_reset(void)
{
   return_down = 0;
   alt_held = 0;
   latched = 0;
}

void alt_enter_note(unsigned code, int down, unsigned modifiers)
{
   if (!is_return(code))
      return;
   if (!down)
   {
      return_down = 0;
      alt_held = 0;
      latched = 0;
      return;
   }
   /* The modifier is in the Return event. On macOS it is not in the event
    * of the Alt key, where flagsChanged contains the AppKit bitfield. */
   return_down = 1;
   alt_held = (modifiers & ALT_ENTER_ALT) ? 1 : 0;
}

int alt_enter_fullscreen_due(void)
{
   if (!(return_down && alt_held) || latched)
      return 0;
   latched = 1;
   return 1;
}

int alt_enter_masks_return(void)
{
   return return_down && alt_held;
}
