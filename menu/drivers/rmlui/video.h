#ifndef RIB_VIDEO_H
#define RIB_VIDEO_H

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* Our brightness and contrast pass after the game's shader (video.c). */

/* The game has loaded, with the shader at its start. We remember that shader
 * as the chosen one, and add our pass when a setting is away from 1.0. */
void rib_video_start(void);
/* The player chose `preset`, or "" for no shader. */
void rib_video_choose(const char *preset);
/* The preset the player chose, without our pass. */
const char *rib_video_chosen(void);
/* After a change to a setting of the pass, we add or leave out the pass, or
 * give it the new values. */
void rib_video_update(void);

RETRO_END_DECLS

#endif
