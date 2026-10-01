#ifndef RMLUI_BRIDGE_H
#define RMLUI_BRIDGE_H
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Existing runloop/menu-driver/save-task hooks. Inside the menu we use the C++
 * interfaces of the components, and no document state passes this facade. */
void rib_rmlui_begin_overlays(void);
/* Events go to text input only while an account form has keyboard focus. */
bool rib_rmlui_begin_native_text(void);
bool rib_rmlui_text_event(bool down, unsigned key, uint32_t character, uint16_t modifiers);
/* The text entry of the menu has the keyboard focus. We type every key sent to
 * it, and no key that is down also counts as a button of the menu pad. */
bool rib_rmlui_typing(void);
/* Call this on every ordinary exit before teardown. False keeps the session open. */
bool rib_rmlui_allow_quit(void);
bool rib_rmlui_overlays_drawing(void);
/* Whether the game waits for the design's splash. Until it is over, we run
 * no core frame in the run loop. */
bool rib_rmlui_game_held(void);
bool rib_rmlui_consume_menu_toggle(void *userdata);
/* The hotkeys, as the player binds them on HOTKEYS
 * (menu/drivers/rmlui/hotkeys.inc). We read them in the RetroArch input code
 * in place of its menu toggle, gamepad combo, RetroPad A and B, and its
 * save state and state slot hotkeys.
 *
 * The keys of MENU, by code, up to `capacity`, for the rule of the menu
 * toggle keys (input/held_key_policy.h), and none while the text entry of the
 * menu has the keyboard. Also whether the pad bindings of MENU are down. */
unsigned rib_rmlui_menu_keys(unsigned *codes, unsigned capacity);
bool rib_rmlui_menu_pad_held(void);
/* The RetroPad buttons of the menu, the first word of the RetroArch input
 * bits, as we read them in the menu: `ok` is CONFIRM down and `cancel` is BACK
 * down, and a position bound to a hotkey for the menu is only that hotkey. We
 * count keys unless the text entry has the keyboard. */
void rib_rmlui_menu_buttons(uint32_t *buttons, unsigned ok, unsigned cancel);
/* Whether a hotkey for the menu is bound to the key `code`. Such a key is only
 * that hotkey, and none of the keys that count as buttons of the menu pad in
 * RetroArch (Space as Start, Backspace as B, in input_driver.c). */
bool rib_rmlui_menu_hotkey_key(unsigned code);
/* Once a frame: the hotkeys for use while the game plays (QUICK SAVE,
 * QUICK LOAD, PREVIOUS SLOT, NEXT SLOT), each once when pressed. We ignore
 * them while the menu is open or the game waits for the splash, and a key
 * that is down then counts only after it is released and pressed again. */
void rib_rmlui_play_hotkeys(void);
void rib_rmlui_notify_state_task(const char *path, int slot, bool is_save, bool success);
static inline bool rib_rmlui_ok_includes_pointer_select(bool is_rmlui)
{
   return !is_rmlui;
}
#ifdef __cplusplus
}
#endif
#endif
