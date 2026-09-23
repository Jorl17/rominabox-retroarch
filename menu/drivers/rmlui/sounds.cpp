#include "sounds.hpp"
#include "host.h"
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
}
