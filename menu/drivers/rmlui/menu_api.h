#ifndef RIB_MENU_API_H
#define RIB_MENU_API_H
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
/* The C userdata belongs to the driver, and the C++ state returned here
 * belongs to the menu. We destroy it before we free the wrapper in RetroArch. */
enum rib_key { RIB_KEY_NOOP, RIB_KEY_UP, RIB_KEY_DOWN, RIB_KEY_LEFT, RIB_KEY_RIGHT, RIB_KEY_OK, RIB_KEY_CANCEL, RIB_KEY_SELECT, RIB_KEY_START, RIB_KEY_TOGGLE, RIB_KEY_RESUME };
void *rib_menu_create(void);
void rib_menu_destroy(void *menu);
void rib_menu_context_destroy(void *menu);
void rib_menu_context_reset(void *menu);
void rib_menu_toggle(void *menu, bool on);
bool rib_menu_consume_toggle(void *menu);
void rib_menu_frame(void *menu, int width, int height);
int rib_menu_key(void *menu, enum rib_key key);
#ifdef __cplusplus
}
#endif
#endif
