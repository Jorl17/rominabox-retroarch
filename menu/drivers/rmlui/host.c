#include "host.h"
#include "files.h"
#include "bind_lines.h"
#include "../../menu_input.h"
#include <gfx/gl_capabilities.h>
#include <features/features_cpu.h>
#include "../../../command.h"
#include "../../../audio/audio_driver.h"
#include "../../../audio/volume_range.h"
#include "../../../configuration.h"
#include "../../../disk_control_interface.h"
#include "../../../file_path_special.h"
#include "../../../runloop.h"
#include "../../../gfx/gfx_thumbnail.h"
#include "../../../gfx/video_driver.h"
#include "../../../gfx/video_shader_parse.h"
#include "../../../input/input_driver.h"
#include "../../../input/input_keymaps.h"
#include "../../../input/input_remapping.h"
#include "../../../verbosity.h"
#include "../../menu_driver.h"
#include "../../menu_cbs.h"
#include <file/file_path.h>
#include <string/stdstring.h>
#include <stdlib.h>
#include <stdio.h>

bool rib_host_menu_open(void)
{
   struct menu_state *menu_st = menu_state_get_ptr();
   return menu_st && (menu_st->flags & MENU_ST_FLAG_ALIVE);
}

/* Keep frames going to the menu driver while the menu is closed. In every video
 * driver we skip the menu while it is closed, so without this we would never
 * draw an overlay. We use the same switch as for drawing the menu, so there is
 * no code for overlays in the video drivers. */
void rib_host_overlay_frames(bool on)
{
   video_driver_state_t *video_st = video_state_get_ptr();
   if (rib_host_menu_open())
      return;
   if (video_st && video_st->poke && video_st->poke->set_texture_enable)
      video_st->poke->set_texture_enable(video_st->data, on, false);
}

/* Resolve through the RetroArch bind table, which includes analog directions. */
bool rib_host_bind_index(const char *id, unsigned *resolved)
{
   unsigned index;
   for (index = 0; index < RARCH_FIRST_META_KEY; ++index)
   {
      const struct input_bind_map *entry = INPUT_CONFIG_BIND_MAP_GET(index);
      if (entry && entry->base && string_is_equal(entry->base, id))
      {
         *resolved = index;
         return true;
      }
   }
   return false;
}

unsigned rib_host_disc_count(void)
{
   rarch_system_info_t *sys_info = &runloop_state_get_ptr()->system;

   if (!disk_control_enabled(&sys_info->disk_control))
      return 0;
   return disk_control_get_num_images(&sys_info->disk_control);
}

void rib_host_restore_keyboard_mapping(void)
{
   unsigned user;
   unsigned bind_index;
   for (user = 0; user < MAX_USERS; ++user)
      for (bind_index = 0; input_config_bind_map_get_valid(bind_index);
           ++bind_index)
         if (input_config_binds[user][bind_index].key != RETROK_UNKNOWN)
            input_keyboard_mapping_bits(1,
                  input_config_binds[user][bind_index].key);
}

/* The path of the core remap file, the one in use when there is no game or
 * content-directory remap (config_load_remap):
 * <input_remapping_directory>/<library name>/<library name>.rmp
 * With sort-by-controller on, we add the name of the physical pad, because
 * the path in use with that setting contains it. */
static bool rib_core_remap_path(char *path, size_t len)
{
   settings_t *settings = config_get_ptr();
   const char *core_name;
   const char *directory;
   char remap_dir[PATH_MAX_LENGTH];

   if (!path || !len)
      return false;
   path[0] = '\0';
   if (!settings)
      return false;

   core_name = runloop_state_get_ptr()->system.info.library_name;
   directory = settings->paths.directory_input_remapping;
   if (!core_name || !*core_name || !directory || !*directory)
      return false;

   strlcpy(remap_dir, core_name, sizeof(remap_dir));
   if (settings->bools.input_remap_sort_by_controller_enable)
   {
      char *device_dir = NULL;
      const char *device_name = input_config_get_device_display_name(
            settings->uints.input_joypad_index[0]);
      if (device_name && *device_name
            && (device_dir = sanitize_path_part(
                  device_name, strlen(device_name)))
            && *device_dir)
         fill_pathname_join_special(remap_dir, core_name, device_dir,
               sizeof(remap_dir));
      free(device_dir);
   }

   fill_pathname_join_special_ext(path, directory, remap_dir, core_name,
         FILE_PATH_REMAP_EXTENSION, len);
   return path[0] != '\0';
}

static bool rib_host_persist_device(unsigned device)
{
   char core_path[PATH_MAX_LENGTH];
   const char *active;
   bool ok;

   if (!rib_core_remap_path(core_path, sizeof(core_path)))
      return false;

   ok = rib_write_remap_device(core_path, device);
   /* A game or content-directory remap, when there is one, comes before the
    * core file and would hide it, so we write the same device into it too. */
   active = runloop_state_get_ptr()->name.remapfile;
   if (active && *active && !string_is_equal(active, core_path))
      ok = rib_write_remap_device(active, device) && ok;
   return ok;
}

bool rib_host_has_settings(void) { return config_get_ptr() != NULL; }

unsigned rib_host_disc_index(void)
{
   return disk_control_get_image_index(&runloop_state_get_ptr()->system.disk_control);
}

void rib_host_disc_label(unsigned index, char *out, size_t length)
{
   disk_control_get_image_label(&runloop_state_get_ptr()->system.disk_control,
         index, out, length);
}

void rib_host_choose_disc(unsigned index)
{
   command_event(CMD_EVENT_DISK_INDEX, &index);
}

bool rib_host_state_path(int slot, char *out, size_t length)
{
   return runloop_get_savestate_path(out, length, slot);
}

bool rib_host_slot_occupied(int slot)
{
   char path[PATH_MAX_LENGTH] = {0};
   return rib_host_state_path(slot, path, sizeof(path)) && path_is_valid(path);
}

void rib_host_thumbnail(int slot, char *out, size_t length)
{
   runloop_state_t *state = runloop_state_get_ptr();
   if (state && state->name.savestate[0])
      gfx_savestate_thumbnail_get_path(out, length, state->name.savestate, slot);
   if (!path_is_valid(out))
      out[0] = '\0';
}

float rib_host_game_aspect(void) { return video_driver_get_core_aspect(); }

void rib_host_select_state_slot(int slot)
{
   settings_t *settings = config_get_ptr();
   if (settings)
      configuration_set_int(settings, settings->ints.state_slot, slot);
}

bool rib_host_save_state(void) { return command_event(CMD_EVENT_SAVE_STATE, NULL); }
bool rib_host_load_state(void) { return command_event(CMD_EVENT_LOAD_STATE, NULL); }
void rib_host_open_menu(void) { if (!rib_host_menu_open()) command_event(CMD_EVENT_MENU_TOGGLE, NULL); }
void rib_host_resume(void) { command_event(CMD_EVENT_MENU_TOGGLE, NULL); }
void rib_host_quit(void) { command_event(CMD_EVENT_QUIT, NULL); }

void rib_host_apply_device(const char *id, unsigned device)
{
   settings_t *settings = config_get_ptr();
   if (!settings)
      return;
   unsigned applied = device ? device : (unsigned)RETRO_DEVICE_JOYPAD;
   configuration_set_uint(settings, settings->uints.input_libretro_device[0], applied);
   command_event(CMD_EVENT_CONTROLLER_INIT, NULL);
   if (!rib_host_persist_device(applied))
      RARCH_ERR("[RIB] controller '%s' is active as device %u, but "
            "the remap could not be written. The next launch will "
            "restore the previous device.\n", id, applied);
   else
      RARCH_LOG("[RIB] controller '%s' applied as device %u.\n", id, applied);
}

float rib_host_volume(void)
{
   settings_t *settings = config_get_ptr();
   return settings ? settings->floats.audio_volume : AUDIO_VOLUME_DEFAULT_DB;
}

bool rib_host_muted(void)
{
   bool *muted = audio_get_bool_ptr(AUDIO_ACTION_MUTE_ENABLE);
   return muted && *muted;
}

void rib_host_set_volume(float db)
{
   settings_t *settings = config_get_ptr();
   bool *muted = audio_get_bool_ptr(AUDIO_ACTION_MUTE_ENABLE);
   if (muted)
      *muted = false;
   if (settings)
      configuration_set_float(settings, settings->floats.audio_volume, db);
   audio_set_float(AUDIO_ACTION_VOLUME_GAIN, db);
}

void rib_host_scroll_sound(bool up)
{
#ifdef HAVE_AUDIOMIXER
   audio_driver_mixer_play_scroll_sound(up);
#else
   (void)up;
#endif
}

void rib_host_ok_sound(void)
{
#ifdef HAVE_AUDIOMIXER
   audio_driver_mixer_play_menu_sound(AUDIO_MIXER_SYSTEM_SLOT_OK);
#endif
}

void rib_host_cancel_sound(void)
{
#ifdef HAVE_AUDIOMIXER
   audio_driver_mixer_play_menu_sound(AUDIO_MIXER_SYSTEM_SLOT_CANCEL);
#endif
}

const char *rib_host_current_shader(void) { return video_shader_get_current_shader_preset(); }

void rib_host_apply_shader(const char *id, const char *path)
{
   settings_t *settings = config_get_ptr();
   bool applied;
   configuration_set_bool(settings, settings->bools.video_shader_enable, path[0] != '\0');
   if (path[0])
      applied = video_shader_apply_shader(settings, video_shader_parse_type(path), path, false);
   else
      applied = video_shader_apply_shader(settings, RARCH_SHADER_NONE, NULL, false);
   fprintf(stderr, "[RIB] shader '%s' %s: %s\n", id,
         applied ? "applied" : "not applied", path[0] ? path : "unfiltered");
}

void rib_host_load_bind(config_file_t *config, const char *id, unsigned index)
{
   char base[64];
   struct config_entry_list *entry;
   struct retro_keybind *bind;

   if (!config || !id)
      return;

   bind = &input_config_binds[0][index];
   snprintf(base, sizeof(base), "input_player1_%s", id);
   entry = config_get_entry(config, base);
   if (entry && entry->value && *entry->value)
   {
      input_keyboard_mapping_bits(0, bind->key);
      bind->key = input_config_translate_str_to_rk(
            entry->value, strlen(entry->value));
      input_keyboard_mapping_bits(1, bind->key);
   }
   input_config_parse_joy_button(base, config, "input_player1",
         id, bind);
   input_config_parse_joy_axis(base, config, "input_player1",
         id, bind);
   input_config_parse_mouse_button(base, config, "input_player1",
         id, bind);
}

static void rib_rmlui_save_joy_button(config_file_t *config,
      const char *key, uint16_t joykey)
{
   char value[32];
   if (joykey == NO_BTN)
      config_set_string(config, key, "nul");
   else if (GET_HAT_DIR(joykey))
   {
      const char *direction = "";
      switch (GET_HAT_DIR(joykey))
      {
         case HAT_UP_MASK: direction = "up"; break;
         case HAT_DOWN_MASK: direction = "down"; break;
         case HAT_LEFT_MASK: direction = "left"; break;
         case HAT_RIGHT_MASK: direction = "right"; break;
         default: break;
      }
      snprintf(value, sizeof(value), "h%u%s", GET_HAT(joykey), direction);
      config_set_string(config, key, value);
   }
   else
      config_set_uint(config, key, joykey);
}

static void rib_rmlui_save_axis(config_file_t *config,
      const char *key, uint32_t axis)
{
   char value[24];
   if (axis == AXIS_NONE)
      config_set_string(config, key, "nul");
   else if (AXIS_NEG_GET(axis) != AXIS_DIR_NONE)
   {
      snprintf(value, sizeof(value), "-%lu",
            (unsigned long)AXIS_NEG_GET(axis));
      config_set_string(config, key, value);
   }
   else
   {
      snprintf(value, sizeof(value), "+%lu",
            (unsigned long)AXIS_POS_GET(axis));
      config_set_string(config, key, value);
   }
}

static void rib_rmlui_save_mouse_button(config_file_t *config,
      const char *key, uint16_t mouse_button)
{
   switch (mouse_button)
   {
      case RETRO_DEVICE_ID_MOUSE_LEFT: config_set_uint(config, key, 1); break;
      case RETRO_DEVICE_ID_MOUSE_RIGHT: config_set_uint(config, key, 2); break;
      case RETRO_DEVICE_ID_MOUSE_MIDDLE: config_set_uint(config, key, 3); break;
      case RETRO_DEVICE_ID_MOUSE_BUTTON_4: config_set_uint(config, key, 4); break;
      case RETRO_DEVICE_ID_MOUSE_BUTTON_5: config_set_uint(config, key, 5); break;
      case RETRO_DEVICE_ID_MOUSE_WHEELUP: config_set_string(config, key, "wu"); break;
      case RETRO_DEVICE_ID_MOUSE_WHEELDOWN: config_set_string(config, key, "wd"); break;
      case RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELUP: config_set_string(config, key, "whu"); break;
      case RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELDOWN: config_set_string(config, key, "whd"); break;
      default: config_set_string(config, key, "nul"); break;
   }
}

void rib_host_clear_bind(unsigned index)
{
   struct retro_keybind *bind = &input_config_binds[0][index];
   input_keyboard_mapping_bits(0, bind->key);
   bind->key = RETROK_UNKNOWN;
   bind->joykey = NO_BTN;
   bind->joyaxis = AXIS_NONE;
   bind->mbutton = NO_BTN;
}

void rib_host_write_bind(config_file_t *config, const char *id, unsigned index)
{
   const struct retro_keybind *bind = &input_config_binds[0][index];
   char key[96];
   char value[64];
   snprintf(key, sizeof(key), "input_player1_%s", id);
   input_keymaps_translate_rk_to_str(bind->key, value, sizeof(value));
   config_set_string(config, key, value);
   snprintf(key, sizeof(key), "input_player1_%s_btn", id);
   rib_rmlui_save_joy_button(config, key, bind->joykey);
   snprintf(key, sizeof(key), "input_player1_%s_axis", id);
   rib_rmlui_save_axis(config, key, bind->joyaxis);
   snprintf(key, sizeof(key), "input_player1_%s_mbtn", id);
   rib_rmlui_save_mouse_button(config, key, bind->mbutton);
}

bool rib_host_bind_conflicts(unsigned left, unsigned right)
{
   const struct retro_keybind *changed = &input_config_binds[0][left];
   const struct retro_keybind *candidate = &input_config_binds[0][right];
   return (changed->key != RETROK_UNKNOWN && changed->key == candidate->key) ||
          (changed->joykey != NO_BTN && changed->joykey == candidate->joykey) ||
          (changed->joyaxis != AXIS_NONE && changed->joyaxis == candidate->joyaxis) ||
          (changed->mbutton != NO_BTN && changed->mbutton == candidate->mbutton);
}

void rib_host_bind_lines(unsigned index, char details[][64], char kinds[][8], int *lines)
{
   rib_lines_from_bind(&input_config_binds[0][index], &input_autoconf_binds[0][index],
         details, kinds, lines);
}

bool rib_host_capture_start(unsigned index, unsigned seconds)
{
   return menu_input_rib_bind_start(index, seconds);
}

void rib_host_capture_cancel(void) { menu_input_rib_bind_cancel(); }

enum rib_capture_result rib_host_capture_poll(bool allow_pointer, float *remaining)
{
   switch (menu_input_rib_bind_poll(menu_driver_get_current_time(), remaining, allow_pointer))
   {
      case MENU_RIB_BIND_CAPTURED: return RIB_CAPTURE_CAPTURED;
      case MENU_RIB_BIND_TIMED_OUT: return RIB_CAPTURE_TIMED_OUT;
      default: return RIB_CAPTURE_PENDING;
   }
}

int64_t rib_host_time_us(void) { return cpu_features_get_time_usec(); }
bool rib_host_core_gl_context(void) { return gl_query_core_context_in_use(); }
rib_pointer rib_host_pointer(void)
{
   menu_input_pointer_t pointer;
   rib_pointer result;
   menu_input_get_pointer_state(&pointer);
   result.x = pointer.x;
   result.y = pointer.y;
   result.pressed = (pointer.flags & MENU_INP_PTR_FLG_PRESSED) != 0;
   return result;
}

bool rib_host_prepare_script_shot(void)
{
   settings_t *settings = config_get_ptr();
   if (!runloop_state_get_ptr() || !video_state_get_ptr())
      return false;
   if (settings)
      configuration_set_bool(settings, settings->bools.video_gpu_screenshot, true);
   return true;
}

void rib_host_end_after_script_shot(const char *path)
{
   runloop_state_t *state = runloop_state_get_ptr();
   state->max_frames = (unsigned)video_state_get_ptr()->frame_count + 2;
   RARCH_LOG("[RIB] menu script shooting %s, exiting after frame %u.\n", path, state->max_frames);
}

void rib_host_script_finished(void)
{
   disk_control_log_core_image(&runloop_state_get_ptr()->system.disk_control, "menu script done");
}
