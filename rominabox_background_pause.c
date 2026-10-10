#include "rominabox_background_pause.h"

bool rib_focused_for_pause(bool window_focused, bool game_has_run)
{
   return window_focused || !game_has_run;
}
