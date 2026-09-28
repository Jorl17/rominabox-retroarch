#pragma once
#include "events.h"
namespace rib {
enum class Sound { None, Confirm, Cancel };
/* We play a sound for every action not listed here. Moving a slider or slot
 * has a separate cue, so a confirm cue would make a second sound. */
inline Sound action_sound(int action)
{
   switch (action)
   {
      case RIB_RMLUI_ACTION_NONE:
      case RIB_RMLUI_ACTION_SLIDER:
      case RIB_RMLUI_ACTION_SELECT_SLOT:
         return Sound::None;
      case RIB_RMLUI_ACTION_RESUME:
      case RIB_RMLUI_ACTION_CONTROLS_BACK:
      case RIB_RMLUI_ACTION_CONTROLS_CANCEL:
      case RIB_RMLUI_ACTION_MENU_CONTROLS_CANCEL:
         return Sound::Cancel;
      default:
         return Sound::Confirm;
   }
}
void play_action_sound(int action);
void play_move_sound(bool up);
/* The player moved a level. Play the movement cue at the volume of the game,
 * and none while that volume is at its silent bottom. */
void play_level_sound(bool up);
/* The cue for a change of level that we add to an export without a menu
 * sound pack. */
void load_level_cue(const char *asset_directory);
}
