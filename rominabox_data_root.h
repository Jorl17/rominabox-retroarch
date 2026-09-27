/* Where we put the folders of a ROM-in-a-Box game. */
#ifndef RIB_DATA_ROOT_H
#define RIB_DATA_ROOT_H

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* Put every folder that RetroArch would otherwise use next to the program or in
 * the user's folders under the game's data folder `root`, and create the config
 * folder there. We call this in the frontend for every platform, and then
 * place what that app contains (cores, filters), when there is any. */
void rib_place_game_directories(const char *root);

RETRO_END_DECLS

#endif
