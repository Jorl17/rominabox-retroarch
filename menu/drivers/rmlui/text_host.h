#ifndef RIB_TEXT_HOST_H
#define RIB_TEXT_HOST_H
#include <stdbool.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Adapter to the RetroArch controller keyboard. Values are borrowed until the
 * next input event. Masking is up to the caller, and we never log the text. */
enum { RIB_KEYBOARD_KEYS = 44, RIB_KEYBOARD_COLUMNS = 11 };
typedef void (*rib_text_complete)(void *context, const char *value);
bool rib_host_keyboard_begin(const char *value, rib_text_complete complete, void *context);
void rib_host_keyboard_end(void);
bool rib_host_keyboard_active(void);
const char *rib_host_keyboard_value(void);
void rib_host_keyboard_replace(const char *value);
const char *rib_host_keyboard_label(unsigned index);
int rib_host_keyboard_focus(void);
void rib_host_keyboard_choose(unsigned index);
void rib_host_text_focus(bool active);
#ifdef __cplusplus
}
#endif
#endif
