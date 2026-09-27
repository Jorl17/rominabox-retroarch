/* ROM-in-a-Box: the DirectInput we call from the input drivers of a Windows
 * game. A sandboxed game has no access to the controllers in DirectInput,
 * so we read them in the launcher (rominabox_pad_relay.h). In the game we
 * then answer for the controllers through a stand-in, from what we read in
 * the launcher, and pass everything else, such as the keyboard and the
 * mouse, to DirectInput itself. */
#ifndef RIB_DINPUT_H
#define RIB_DINPUT_H

#include <dinput.h>

/* `real` wrapped in the stand-in when we relay the controllers through the
 * launcher, or `real` itself otherwise. */
LPDIRECTINPUT8 rib_dinput_for_game(LPDIRECTINPUT8 real);

#endif
