#ifndef RMLUI_BRIDGE_H
#define RMLUI_BRIDGE_H
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Existing runloop/menu-driver/save-task hooks. Inside the menu we use the C++
 * interfaces of the components, and no document state passes this facade. */
void rib_rmlui_begin_overlays(void);
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
