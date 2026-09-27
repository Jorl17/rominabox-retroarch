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
bool rib_rmlui_consume_menu_toggle(void *userdata);
void rib_rmlui_notify_state_task(const char *path, int slot, bool is_save, bool success);
static inline bool rib_rmlui_ok_includes_pointer_select(bool is_rmlui)
{
   return !is_rmlui;
}
#ifdef __cplusplus
}
#endif
#endif
