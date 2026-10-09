/* ROM-in-a-Box: what happened when the player exported or imported the
 * game's data from the menu (menu/drivers/rmlui/game_data_host.h). On
 * Windows the launcher does both, outside the game's sandbox, and passes
 * this back through the controller relay (rominabox_pad_relay.h). The
 * launcher includes this too. */
#ifndef RIB_GAME_DATA_ANSWER_H
#define RIB_GAME_DATA_ANSWER_H

typedef enum
{
   /* Done. For a zip chosen to import, the data of this game. */
   RIB_DATA_DONE = 0,
   /* A zip chosen to import with the data of another game for the same
    * console, which we import after asking. */
   RIB_DATA_OTHER_GAME,
   /* The player closed the file panel without choosing. */
   RIB_DATA_CANCELLED,
   /* We could not, or we will not, for the reason in the sentence. */
   RIB_DATA_FAILED
} rib_data_answer;

/* When the launcher finds this file in the game's data after the game has
 * closed, it removes it and starts the game again, which imports the zip
 * set aside (RIB_GAME_FILE(PendingImport) in launch_contract.inc). */
#define RIB_DATA_RESTART_MARKER "restart-after-import"

#endif
