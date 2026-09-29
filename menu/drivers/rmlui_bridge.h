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
/* The menu actions, as the player binds them on MENU CONTROLS
 * (menu/drivers/rmlui/menu_controls.inc). We read them in the RetroArch input
 * code in place of its menu toggle, gamepad combo and RetroPad A and B.
 *
 * The keys of MENU, by code, up to `capacity`, for the rule of the menu
 * toggle keys (input/held_key_policy.h), and none while the text entry of the
 * menu has the keyboard. Also whether the pad bindings of MENU are down. */
unsigned rib_rmlui_menu_keys(unsigned *codes, unsigned capacity);
bool rib_rmlui_menu_pad_held(void);
/* The RetroPad buttons of the menu, the first word of the RetroArch input
 * bits, as we read them in the menu: `ok` is CONFIRM down and `cancel` is BACK
 * down, and a position bound to one of the actions is only that action. We
 * count keys unless the text entry has the keyboard. */
void rib_rmlui_menu_buttons(uint32_t *buttons, unsigned ok, unsigned cancel);
void rib_rmlui_notify_state_task(const char *path, int slot, bool is_save, bool success);
static inline bool rib_rmlui_ok_includes_pointer_select(bool is_rmlui)
{
   return !is_rmlui;
}
#ifdef __cplusplus
}
#endif
#endif
