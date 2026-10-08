#include "host.h"
#include "files.h"
#include "bind_lines.h"
#include "pad_inputs.h"
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
#ifdef HAVE_AUDIOMIXER
#include "../../../tasks/task_audio_mixer.h"
#endif
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
#include <streams/file_stream.h>
#include <string/stdstring.h>
#include "../../../rominabox_environment.h"
#include "../../../rominabox_launch.h"
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
         if (RETRO_KEYBIND_KEY(&input_config_binds[user][bind_index]) != RETROK_UNKNOWN)
            input_keyboard_mapping_bits(1,
                  RETRO_KEYBIND_KEY(&input_config_binds[user][bind_index]));
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

/* The file of the picture of `slot`, whether it exists or not. It is the
 * file we write after a save of that slot. */
static void rib_picture_path(int slot, char *out, size_t length)
{
   runloop_state_t *state = runloop_state_get_ptr();
   out[0] = '\0';
   if (state && state->name.savestate[0])
      gfx_savestate_thumbnail_get_path(out, length, state->name.savestate, slot);
}

void rib_host_thumbnail(int slot, char *out, size_t length)
{
   rib_picture_path(slot, out, length);
   if (!path_is_valid(out))
      out[0] = '\0';
}

bool rib_host_copy_picture(int from, int to)
{
   char source[PATH_MAX_LENGTH];
   char target[PATH_MAX_LENGTH];
   void *bytes  = NULL;
   int64_t size = 0;
   bool copied;
   rib_picture_path(from, source, sizeof(source));
   rib_picture_path(to, target, sizeof(target));
   if (!*source || !*target || !filestream_read_file(source, &bytes, &size))
      return false;
   copied = filestream_write_file(target, bytes, size);
   free(bytes);
   if (!copied)
      RARCH_ERR("[RIB] could not copy the picture of slot %d to %s.\n", from, target);
   return copied;
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
void rib_host_toggle_fullscreen(void) { command_event(CMD_EVENT_FULLSCREEN_TOGGLE, NULL); }
void rib_host_show_pointer(bool menu_open)
{
   video_driver_state_t *video_st = video_state_get_ptr();
   if (!config_get_ptr()->bools.video_fullscreen)
      return;
   if (video_st->poke && video_st->poke->show_mouse)
      video_st->poke->show_mouse(video_st->data, menu_open);
}
void rib_host_quit(void) { command_event(CMD_EVENT_QUIT, NULL); }
void rib_host_restart(void) { command_event(CMD_EVENT_RESET, NULL); }

void rib_host_forget(void)
{
   char *data = rib_data_directory();
   char marker[PATH_MAX_LENGTH];
   RFILE *file = NULL;
   if (data)
   {
      fill_pathname_join_special(marker, data, RIB_FORGET_MARKER, sizeof(marker));
      file = filestream_open(marker, RETRO_VFS_FILE_ACCESS_WRITE, RETRO_VFS_FILE_ACCESS_HINT_NONE);
   }
   free(data);
   if (!file)
   {
      RARCH_ERR("[RIB] could not mark the game to be forgotten; it plays on.\n");
      return;
   }
   filestream_close(file);
   RARCH_LOG("[RIB] the game will be forgotten once it has closed.\n");
   command_event(CMD_EVENT_QUIT, NULL);
}

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

/* The player settings that we apply in RetroArch while the game runs: for
 * each key declared in settings.inc, how we read it and how we apply it.
 *
 * The quiet end of the volume is silence, so for a muted game we show the
 * quiet end, and choosing a level turns mute off. */
static float rib_host_volume_now(settings_t *settings)
{
   bool *muted = audio_get_bool_ptr(AUDIO_ACTION_MUTE_ENABLE);
   return muted && *muted ? AUDIO_VOLUME_MIN_DB : settings->floats.audio_volume;
}

static float rib_host_read_AudioVolume(settings_t *settings)
{
   return rib_host_volume_now(settings);
}

static void rib_host_apply_AudioVolume(settings_t *settings, float value)
{
   bool *muted = audio_get_bool_ptr(AUDIO_ACTION_MUTE_ENABLE);
   if (muted)
      *muted = false;
   configuration_set_float(settings, settings->floats.audio_volume, value);
   audio_set_float(AUDIO_ACTION_VOLUME_GAIN, value);
}

static float rib_host_read_PauseNonactive(settings_t *settings)
{
   return settings->bools.pause_nonactive ? 1.0f : 0.0f;
}

static void rib_host_apply_PauseNonactive(settings_t *settings, float value)
{
   configuration_set_bool(settings, settings->bools.pause_nonactive, value != 0.0f);
}

static float rib_host_read_InputRumbleEnable(settings_t *settings)
{
   return settings->bools.input_rumble_enable ? 1.0f : 0.0f;
}

static void rib_host_apply_InputRumbleEnable(settings_t *settings, float value)
{
   configuration_set_bool(settings, settings->bools.input_rumble_enable, value != 0.0f);
}

static float rib_host_read_FastforwardRatio(settings_t *settings)
{
   return settings->floats.fastforward_ratio;
}

/* A new speed applies at once to a fast forward that is already running,
 * because RetroArch reads the ratio when it sets the frame limit. */
static void rib_host_apply_FastforwardRatio(settings_t *settings, float value)
{
   configuration_set_float(settings, settings->floats.fastforward_ratio, value);
   command_event(CMD_EVENT_SET_FRAME_LIMIT, NULL);
}

/* Whether each key applies to the running game. Every game has a volume and
 * a window, but we can rumble a pad only when the core uses the rumble
 * interface. */
static bool rib_host_used_AudioVolume(void) { return true; }
static bool rib_host_used_PauseNonactive(void) { return true; }
/* We declare the fast forward speed in an export only for a game with it. */
static bool rib_host_used_FastforwardRatio(void) { return true; }
static bool rib_host_used_InputRumbleEnable(void)
{
   return runloop_state_get_ptr()->system.supports_rumble;
}

/* Every key declared in settings.inc has its functions above. For a key
 * declared there and not handled here, the build stops on the name of a
 * function that does not exist. */
bool rib_host_setting(enum rib_setting_key key, float *value)
{
   settings_t *settings = config_get_ptr();
   if (!settings || !value)
      return false;
   switch (key)
   {
#define RIB_SETTING_KEY(name, retroarch) \
      case RIB_SETTING_##name: *value = rib_host_read_##name(settings); return true;
#include "settings.inc"
      default:
         return false;
   }
}

bool rib_host_set_setting(enum rib_setting_key key, float value)
{
   settings_t *settings = config_get_ptr();
   if (!settings)
      return false;
   switch (key)
   {
#define RIB_SETTING_KEY(name, retroarch) \
      case RIB_SETTING_##name: rib_host_apply_##name(settings, value); return true;
#include "settings.inc"
      default:
         return false;
   }
}

bool rib_host_setting_used(enum rib_setting_key key)
{
   switch (key)
   {
#define RIB_SETTING_KEY(name, retroarch) \
      case RIB_SETTING_##name: return rib_host_used_##name();
#include "settings.inc"
      default:
         return false;
   }
}

void rib_host_level_sound(bool up)
{
#ifdef HAVE_AUDIOMIXER
   settings_t *settings = config_get_ptr();
   const unsigned slot  = up ? AUDIO_MIXER_SYSTEM_SLOT_UP : AUDIO_MIXER_SYSTEM_SLOT_DOWN;
   if (!settings)
      return;
   audio_driver_mixer_play_menu_sound(slot);
   /* A new voice starts at unity. We set its level before the next mixer
    * run, on this thread, so no sample of it plays louder. In the menu we
    * play no cue at the silent bottom (sounds.cpp). */
   audio_driver_mixer_set_stream_volume(slot, rib_host_volume_now(settings));
#else
   (void)up;
#endif
}

void rib_host_load_level_cue(const char *path)
{
#ifdef HAVE_AUDIOMIXER
   settings_t *settings = config_get_ptr();
   /* With audio off the mixer in RetroArch has no output rate, and a sound
    * loaded into it is resampled to 0 Hz past the end of its buffer. */
   if (!settings || !settings->bools.audio_enable || !path || !path_is_valid(path))
      return;
   task_push_audio_mixer_load(path, NULL, NULL, true,
         AUDIO_MIXER_SLOT_SELECTION_MANUAL, AUDIO_MIXER_SYSTEM_SLOT_UP);
   task_push_audio_mixer_load(path, NULL, NULL, true,
         AUDIO_MIXER_SLOT_SELECTION_MANUAL, AUDIO_MIXER_SYSTEM_SLOT_DOWN);
#else
   (void)path;
#endif
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
   RARCH_LOG("[RIB] shader '%s' %s: %s\n", id,
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
      input_keyboard_mapping_bits(0, RETRO_KEYBIND_KEY(bind));
      RETRO_KEYBIND_SET_KEY(bind, input_config_translate_str_to_rk(
            entry->value, strlen(entry->value)));
      input_keyboard_mapping_bits(1, RETRO_KEYBIND_KEY(bind));
   }
   input_config_parse_joy_button(base, config, "input_player1",
         id, bind, &input_config_bind_labels[0][index]);
   input_config_parse_joy_axis(base, config, "input_player1",
         id, bind, &input_config_bind_labels[0][index]);
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
#define RIB_MOUSE_BUTTON(value, id, word) case id: config_set_string(config, key, value); break;
#include "mouse_buttons.inc"
      default: config_set_string(config, key, "nul"); break;
   }
}

void rib_host_clear_bind(unsigned index)
{
   struct retro_keybind *bind = &input_config_binds[0][index];
   input_keyboard_mapping_bits(0, RETRO_KEYBIND_KEY(bind));
   RETRO_KEYBIND_SET_KEY(bind, RETROK_UNKNOWN);
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
   input_keymaps_translate_rk_to_str(RETRO_KEYBIND_KEY(bind), value, sizeof(value));
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
   return (RETRO_KEYBIND_KEY(changed) != RETROK_UNKNOWN
            && RETRO_KEYBIND_KEY(changed) == RETRO_KEYBIND_KEY(candidate)) ||
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

bool rib_host_key_code(const char *name, unsigned *code)
{
   enum retro_key key;
   if (!name || !*name || !code)
      return false;
   key = input_config_translate_str_to_rk(name, strlen(name));
   if (key == RETROK_UNKNOWN)
      return false;
   *code = (unsigned)key;
   return true;
}

#ifdef RIB_MENU_SCRIPT
/* The key we keep down for the test script, and the frame on which we
 * release it. */
static unsigned rib_script_key;
static uint64_t rib_script_key_until;

bool rib_host_script_press(const char *name)
{
   unsigned code;
   if (!rib_host_key_code(name, &code))
      return false;
   rib_script_key       = code;
   rib_script_key_until = video_state_get_ptr()->frame_count + 2;
   return true;
}
#endif

bool rib_host_bind_key(unsigned index, unsigned *code)
{
   const enum retro_key key = RETRO_KEYBIND_KEY(&input_config_binds[0][index]);
   if (!code || key == RETROK_UNKNOWN)
      return false;
   *code = (unsigned)key;
   return true;
}

bool rib_host_key_down(unsigned code)
{
#ifdef RIB_MENU_SCRIPT
   if (code == rib_script_key && video_state_get_ptr()->frame_count < rib_script_key_until)
      return true;
#endif
   return input_driver_keyboard_pressed(code) != 0;
}
bool rib_host_pad_input(const char *id, unsigned *bind) { return rib_pad_input_bind(id, bind); }
bool rib_host_pad_down(unsigned bind) { return rib_pad_input_down(bind); }

/* Where we write a capture for a hotkey. It is none of
 * the RetroArch binds, so no input of the game changes while it runs. */
static struct retro_keybind rib_captured_input;

bool rib_host_capture_input_start(unsigned seconds)
{
   return menu_input_rib_capture_start(&rib_captured_input, seconds);
}

void rib_host_captured_input(char *binding, size_t length)
{
#define RIB_HOTKEY_BINDING(name, prefix) static const char name##_prefix[] = prefix;
#include "hotkeys.inc"
   const struct retro_keybind *input = &rib_captured_input;
   char key[64];
   unsigned bind;

   if (!binding || !length)
      return;
   binding[0] = '\0';
   /* During the capture we marked the key as used by a bind, but none uses it. */
   if (RETRO_KEYBIND_KEY(input) != RETROK_UNKNOWN)
   {
      input_keyboard_mapping_bits(0, RETRO_KEYBIND_KEY(input));
      rib_host_restore_keyboard_mapping();
      key[0] = '\0';
      input_keymaps_translate_rk_to_str(RETRO_KEYBIND_KEY(input), key, sizeof(key));
      if (key[0] && !string_is_equal(key, "nul"))
         snprintf(binding, length, "%s%s", Key_prefix, key);
      return;
   }
   rib_host_restore_keyboard_mapping();
   if (rib_pad_input_of(input->joykey, input->joyaxis, &bind))
      snprintf(binding, length, "%s%s", Pad_prefix, rib_pad_input_id(bind));
}

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
uintptr_t rib_host_native_window(void) { return video_driver_window_get(); }
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

#ifdef RIB_MENU_SCRIPT
/* The test script driver's picture and exit, in a test build only. */
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
#endif
