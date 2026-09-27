/* Where we put the folders of a ROM-in-a-Box game: the RetroArch names, all
 * under the game's data folder. */
#include "rominabox_data_root.h"

#include <file/file_path.h>

#include "defaults.h"

static const struct
{
   enum default_dirs dir;
   const char *name;
} rib_game_directories[] = {
   { DEFAULT_DIR_LOGS,            "logs" },
   { DEFAULT_DIR_PLAYLIST,        "playlists" },
   { DEFAULT_DIR_RECORD_OUTPUT,   "records" },
   { DEFAULT_DIR_RECORD_CONFIG,   "records_config" },
   { DEFAULT_DIR_SRAM,            "saves" },
   { DEFAULT_DIR_SCREENSHOT,      "screenshots" },
   { DEFAULT_DIR_SAVESTATE,       "states" },
   { DEFAULT_DIR_SYSTEM,          "system" },
   { DEFAULT_DIR_ASSETS,          "assets" },
   { DEFAULT_DIR_AUTOCONFIG,      "autoconfig" },
   { DEFAULT_DIR_CHEATS,          "cht" },
   { DEFAULT_DIR_MENU_CONFIG,     "config" },
   { DEFAULT_DIR_REMAP,           "config/remaps" },
   { DEFAULT_DIR_CORE,            "cores" },
   { DEFAULT_DIR_DATABASE,        "database/rdb" },
   { DEFAULT_DIR_CORE_ASSETS,     "downloads" },
   { DEFAULT_DIR_AUDIO_FILTER,    "filters/audio" },
   { DEFAULT_DIR_VIDEO_FILTER,    "filters/video" },
   { DEFAULT_DIR_CORE_INFO,       "info" },
   { DEFAULT_DIR_OVERLAY,         "overlays" },
   { DEFAULT_DIR_OSK_OVERLAY,     "overlays/keyboards" },
   { DEFAULT_DIR_SHADER,          "shaders" },
   { DEFAULT_DIR_THUMBNAILS,      "thumbnails" },
   { DEFAULT_DIR_CACHE,           "cache" },
};

void rib_place_game_directories(const char *root)
{
   size_t i;
   for (i = 0; i < sizeof(rib_game_directories) / sizeof(rib_game_directories[0]); i++)
      fill_pathname_join(g_defaults.dirs[rib_game_directories[i].dir], root,
            rib_game_directories[i].name, sizeof(g_defaults.dirs[0]));
   if (!path_is_directory(g_defaults.dirs[DEFAULT_DIR_MENU_CONFIG]))
      path_mkdir(g_defaults.dirs[DEFAULT_DIR_MENU_CONFIG]);
}
