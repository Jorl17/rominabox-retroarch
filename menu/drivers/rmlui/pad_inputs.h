#ifndef RIB_MENU_PAD_INPUTS_H
#define RIB_MENU_PAD_INPUTS_H

#include <stdbool.h>
#include <stddef.h>
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
/* The pad input of a captured button (`joykey`, including the direction of
 * a hat) or axis (`joyaxis`) on the pad of the first player, through its
 * profile. False when it is no position and not the menu button in that
 * profile. */
bool rib_pad_input_of(uint16_t joykey, uint32_t joyaxis, unsigned *bind);
/* As rib_pad_input_of, through the profile of the pad at joypad index `pad`. */
bool rib_pad_input_on(unsigned pad, uint16_t joykey, uint32_t joyaxis, unsigned *bind);
/* A pad input in the form of RetroArch's config: "13" for a button, "h0up"
 * for a direction of a hat, "+3" or "-3" for an axis. We write it to
 * `value`, of at least RIB_PAD_INPUT_VALUE_MAX bytes, and return false when
 * there is no input. */
#define RIB_PAD_INPUT_VALUE_MAX 16
bool rib_pad_input_value(uint16_t joykey, uint32_t joyaxis, char *value, size_t size);
/* The input in `value`, in that form. False for any other text. */
bool rib_pad_input_parse(const char *value, uint16_t *joykey, uint32_t *joyaxis);
/* The name of the position or Home `bind` in the profile of the first
 * player's pad, when the profile has one. */
bool rib_pad_input_name(unsigned bind, char *name, size_t size);
/* Start capturing into `output` an input for a hotkey, for `seconds`. */
struct retro_keybind;
bool rib_pad_input_capture_start(struct retro_keybind *output, unsigned seconds);
/* Start capturing an input into the bind `index` of player 1, on CONTROLS,
 * for `seconds`. */
bool rib_pad_input_bind_start(unsigned index, unsigned seconds);
/* The joypad index of the pad we last captured from. */
unsigned rib_pad_input_captured_pad(void);

#ifdef __cplusplus
}
#endif
#endif
