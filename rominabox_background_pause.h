#ifndef ROMINABOX_BACKGROUND_PAUSE_H
#define ROMINABOX_BACKGROUND_PAUSE_H

#include <boolean.h>

/* Whether the game window counts as focused when the player keeps the game
 * from playing in the background (pause_nonactive). Without focus, the game
 * pauses, but not before it has run: its splash and first frame are drawn
 * first, so what we draw over a paused game has a frame to go over. */
bool rib_focused_for_pause(bool window_focused, bool game_has_run);

#endif
