/* ROM-in-a-Box: the DirectInput we call from the input drivers of a Windows
 * game. A sandboxed game has no access to the controllers in DirectInput,
 * so we read them in the launcher (rominabox_pad_relay.h). In the game we
 * then answer for the controllers through a stand-in, from what we read in
 * the launcher, and pass everything else, such as the keyboard and the
 * mouse, to DirectInput itself. */
#ifndef RIB_DINPUT_H
#define RIB_DINPUT_H

#include <stddef.h>
#include <dinput.h>

#include <retro_common_api.h>

#include "../../rominabox_game_data.h"

RETRO_BEGIN_DECLS

/* `real` wrapped in the stand-in when we relay the controllers through the
 * launcher, or `real` itself otherwise. */
LPDIRECTINPUT8 rib_dinput_for_game(LPDIRECTINPUT8 real);

/* Ask the launcher, outside the sandbox, to do `what` for the game's data
 * (RIB_PAD_RELAY_EXPORT_DATA and those after it), and wait for its answer,
 * however long the player spends in its file dialog. */
rib_data_answer rib_pad_relay_game_data(int what, char *title, size_t title_size,
      char *sentence, size_t sentence_size);

RETRO_END_DECLS

#endif
