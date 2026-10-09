#ifndef RIB_MENU_PAD_INPUTS_H
#define RIB_MENU_PAD_INPUTS_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The pad inputs in a binding to a hotkey (hotkeys.inc). Each is a position
 * of the standard pad, or Home, the menu button of the pad. Each is a
 * RetroArch bind, and we read it from every pad assigned to player 1 through
 * the buttons for that bind in its RetroArch profile, never through the
 * button numbers of the pad, so we read any pad with a profile the same way. */

/* The bind with the name `id`: the bind of a position (input_player1_<id>), or for
 * Home the bind for the menu button in the profile of the pad. Returns false
 * for any other id, including the direction of a stick. */
bool rib_pad_input_bind(const char *id, unsigned *bind);
/* The id of `bind`, or NULL when it is not a pad input. */
const char *rib_pad_input_id(unsigned bind);
/* Whether `bind` is down on any pad assigned to player 1, through its
 * profile. False for a pad whose profile has no button for it. */
bool rib_pad_input_down(unsigned bind);
/* The pad input that a captured button (`joykey`, including the direction of
 * a hat) or axis (`joyaxis`) is on the pad of the first player, through its
 * profile. False when the profile maps it to no position and no menu
 * button. */
bool rib_pad_input_of(uint16_t joykey, uint32_t joyaxis, unsigned *bind);
/* Start capturing into `output` an input for a hotkey, for `seconds`. */
struct retro_keybind;
bool rib_pad_input_capture_start(struct retro_keybind *output, unsigned seconds);

#ifdef __cplusplus
}
#endif
#endif
