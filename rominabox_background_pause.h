#ifndef ROMINABOX_BACKGROUND_PAUSE_H
#define ROMINABOX_BACKGROUND_PAUSE_H

#include <boolean.h>

/* Whether the game window counts as focused when the player keeps the game
 * from playing in the background (pause_nonactive). Without focus, the game
 * pauses. */
bool rib_focused_for_pause(bool window_focused, bool game_has_run);

#endif
