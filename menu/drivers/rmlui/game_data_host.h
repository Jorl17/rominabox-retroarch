/* What DATA asks of the platform: a file panel, and the zips of the game's
 * data. On macOS the player does both (game_data_macos.mm). On Windows the
 * launcher does both outside the game's sandbox (game_data_windows.cpp).
 * Each fills `sentence` with the reason when it answers RIB_DATA_FAILED. */
#ifndef RIB_GAME_DATA_HOST_H
#define RIB_GAME_DATA_HOST_H

#include <stddef.h>

#include "../../../rominabox_game_data.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Ask where to save the zip, and write the game's data there. */
rib_data_answer rib_game_data_export_to_file(char *sentence, size_t sentence_size);
/* Ask for a zip to import and check it against this game. For another
 * game's data, `title` is that game's title. */
rib_data_answer rib_game_data_choose_import(char *title, size_t title_size,
      char *sentence, size_t sentence_size);
/* Set the chosen zip aside for the launcher, which imports it when the game
 * starts again. We close the game after this, and the launcher starts it
 * again (RIB_DATA_RESTART_MARKER). */
rib_data_answer rib_game_data_confirm_import(char *sentence, size_t sentence_size);

#ifdef __cplusplus
}
#endif

#endif
