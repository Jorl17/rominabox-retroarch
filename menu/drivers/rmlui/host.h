#ifndef RIB_MENU_HOST_H
#define RIB_MENU_HOST_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Calls into the running RetroArch. We keep the state of each menu feature in
 * the feature, and here we use RetroArch settings and commands as they are. */
bool rib_host_menu_open(void);
void rib_host_overlay_frames(bool on);
bool rib_host_has_settings(void);
bool rib_host_bind_index(const char *id, unsigned *resolved);
void rib_host_restore_keyboard_mapping(void);
void rib_host_apply_device(const char *id, unsigned device);
unsigned rib_host_disc_count(void);
unsigned rib_host_disc_index(void);
void rib_host_disc_label(unsigned index, char *out, size_t length);
void rib_host_choose_disc(unsigned index);
bool rib_host_state_path(int slot, char *out, size_t length);
bool rib_host_slot_occupied(int slot);
/* Pass an empty output buffer. We leave it empty when there is no thumbnail. */
void rib_host_thumbnail(int slot, char *out, size_t length);
float rib_host_game_aspect(void);
void rib_host_select_state_slot(int slot);
bool rib_host_save_state(void);
bool rib_host_load_state(void);
void rib_host_resume(void);
void rib_host_quit(void);
float rib_host_volume(void);
bool rib_host_muted(void);
void rib_host_set_volume(float db);
void rib_host_scroll_sound(bool up);
void rib_host_ok_sound(void);
void rib_host_cancel_sound(void);
const char *rib_host_current_shader(void);
/* Check rib_host_has_settings() before selecting, since this uses settings. */
void rib_host_apply_shader(const char *id, const char *path);

#ifdef __cplusplus
}
#endif
#endif
