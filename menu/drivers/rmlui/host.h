#ifndef RIB_MENU_HOST_H
#define RIB_MENU_HOST_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct config_file;
enum { RIB_HOST_BIND_LINE_MAX = 64 };

#ifdef __cplusplus
extern "C" {
#endif

/* Calls into the running RetroArch. We keep the state of each menu feature in
 * the feature, and here we use RetroArch settings and commands as they are. */
/* We use the config objects only during this call. Call it after loading
 * every declared bind, so that we restore the mappings once. */
void rib_host_load_bind(struct config_file *config, const char *id, unsigned index);
void rib_host_clear_bind(unsigned index);
void rib_host_write_bind(struct config_file *config, const char *id, unsigned index);
bool rib_host_bind_conflicts(unsigned left, unsigned right);
void rib_host_bind_lines(unsigned index, char details[][64], char kinds[][8], int *lines);
enum rib_capture_result { RIB_CAPTURE_PENDING, RIB_CAPTURE_CAPTURED, RIB_CAPTURE_TIMED_OUT };
bool rib_host_capture_start(unsigned index, unsigned seconds);
void rib_host_capture_cancel(void);
enum rib_capture_result rib_host_capture_poll(bool allow_pointer, float *remaining);
typedef struct rib_pointer { int x, y; bool pressed; } rib_pointer;
rib_pointer rib_host_pointer(void);
int64_t rib_host_time_us(void);
bool rib_host_core_gl_context(void);
/* For test scripts only. Take the capture before the usual frame-limit exit. */
bool rib_host_prepare_script_shot(void);
void rib_host_end_after_script_shot(const char *path);
void rib_host_script_finished(void);
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
void rib_host_open_menu(void);
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
