#include "sounds.hpp"
#include "host.h"
#include "../../../audio/volume_range.h"
#include <file/file_path.h>
#include <retro_miscellaneous.h>
#include <cstdio>
namespace rib {
void play_action_sound(int action)
{
#ifdef HAVE_AUDIOMIXER
   switch (action_sound(action))
   {
      case Sound::Confirm: rib_host_ok_sound(); break;
      case Sound::Cancel: rib_host_cancel_sound(); break;
      case Sound::None: break;
   }
#endif
}
void play_move_sound(bool up)
{
#ifdef HAVE_AUDIOMIXER
   rib_host_scroll_sound(up);
#endif
}
void play_level_sound(bool up)
{
#ifdef HAVE_AUDIOMIXER
   rib_host_level_sound(up);
#else
   (void)up;
#endif
}
void load_level_cue(const char *asset_directory)
{
#ifdef HAVE_AUDIOMIXER
   char path[PATH_MAX_LENGTH];
   if (!asset_directory || !*asset_directory)
      return;
   snprintf(path, sizeof(path), "%s/%s", asset_directory, RIB_VOLUME_TICK_FILE);
   if (path_is_valid(path))
      rib_host_load_level_cue(path);
#else
   (void)asset_directory;
#endif
}
}
