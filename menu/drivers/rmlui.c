#include <stdlib.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "../../command.h"
#include "../../audio/audio_driver.h"
#include "../../configuration.h"
#include "../../gfx/gfx_thumbnail.h"
#include "../../input/input_driver.h"
#include "../../input/input_keymaps.h"
#include "../../input/input_remapping.h"
#include "../../file_path_special.h"
#include "../../runloop.h"
#include "../../gfx/video_driver.h"
#include "../../verbosity.h"
#include <file/file_path.h>
#include <file/config_file.h>
#include <streams/file_stream.h>
#include <string/stdstring.h>
#include <libretro.h>
#include "../menu_driver.h"
#include "../menu_input.h"
#include "../menu_cbs.h"
#include "rmlui_bridge.h"

#ifndef RIB_RMLUI_DEFAULT_ASSETS
#define RIB_RMLUI_DEFAULT_ASSETS "."
#endif

/* The size of a buffer. The controls that exist are the ones the console
 * declares, and this number is only how many of them fit in this struct.
 * When a console declares more, we say so in the log. */
#define RIB_CONTROL_MAX 48

/* How many controllers fit in a picker. Like the limit for controls, this is
 * the size of a buffer. The controllers that exist are the ones in the console
 * package, and when there are more, we say so in the log. */
#define RIB_DEVICE_MAX 8
#define RIB_CONTROL_CAPTURE_SECONDS 10

typedef struct rib_control
{
   char id[32];
   unsigned bind_index;
} rib_control_t;

typedef struct rib_rmlui_menu
{
   bool initialized;
   bool pointer_pressed;
   bool transfer_pending;
   bool transfer_is_save;
   int selected_slot;
   int transfer_slot;
   int focused;
   bool controls_visible;
   bool controls_loaded;
   int panel_focus;
   char screen[32];
   char volume_path[PATH_MAX_LENGTH];
   bool capture_active;
   int capture_control;
   int control_focus;
   int selected_control;
   char profile_id[32];
   /* The controllers in the picker, from the exported configuration. */
   char device_ids[RIB_DEVICE_MAX][32];
   char device_names[RIB_DEVICE_MAX][NAME_MAX_LENGTH];
   /* The emulated device for each variant. 0 is the core's default device,
    * which suits most pads. */
   unsigned device_libretro[RIB_DEVICE_MAX];
   int device_count;
   bool device_picker_open;
   rib_control_t controls[RIB_CONTROL_MAX];
   int control_count;
   bool control_active[RIB_CONTROL_MAX];
   bool capture_ignore_pointer;
   char transfer_path[PATH_MAX_LENGTH];
   char controls_path[PATH_MAX_LENGTH];
   char control_labels[RIB_CONTROL_MAX][NAME_MAX_LENGTH];
   char default_labels[RIB_CONTROL_MAX][NAME_MAX_LENGTH];
   struct retro_keybind default_binds[RIB_CONTROL_MAX];
} rib_rmlui_menu_t;

static bool rib_splash_active;
static bool rib_splash_keep_menu_open;
static retro_time_t rib_splash_started_at;
static rib_rmlui_menu_t *rib_rmlui_active_menu;

static void rib_rmlui_refresh_controls(rib_rmlui_menu_t *menu);
static void rib_rmlui_cancel_capture(rib_rmlui_menu_t *menu, const char *status);

void rib_rmlui_begin_splash(bool keep_menu_open)
{
   rib_splash_active = true;
   rib_splash_keep_menu_open = keep_menu_open;
   rib_splash_started_at = 0;
}

bool rib_rmlui_splash_active(void)
{
   return rib_splash_active;
}

/* Resolve a declared control id to its libretro bind.
 *
 * We take the mapping from RetroArch: input_config_bind_map is generated from
 * the same DECLARE_BIND table as the analogue directions, so it contains
 * l_x_plus as well as up. A separate table here could bind a stick to the
 * button at the same index, because the analogue directions are in a
 * different range of indexes.
 */
static bool rib_control_bind_index(const char *id, unsigned *resolved)
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

/* Take the control list from the exported configuration.
 *
 * We write that file from the console package at export, so it contains
 * exactly the controls the console declares, in the order of the declaration,
 * and the player moves through them in that order. Because we read the list
 * here, we can add a control to a console without rebuilding the player, and
 * the list has a single source.
 */
/* Whether a control is part of the pad in use now.
 *
 * We export the labels of every pad in the picker, because a player who
 * changes pad should see the new labels at once, and an exported game has no
 * other place to find them. With this filter, a player on a three-button pad
 * cannot move to the extra buttons of a six-button pad, which have no element
 * in its scene. For a console with one pad we declare no list, and then every
 * control is part of the pad.
 */
static bool rib_rmlui_control_belongs(const char *list, const char *id)
{
   const char *at;
   size_t length;

   if (!list || !*list)
      return true;
   length = strlen(id);
   for (at = list; (at = strstr(at, id)); at += length)
   {
      const bool starts = (at == list) || at[-1] == ' ';
      const bool ends = at[length] == '\0' || at[length] == ' ';
      if (starts && ends)
         return true;
   }
   return false;
}

static void rib_rmlui_discover_controls(rib_rmlui_menu_t *menu,
      config_file_t *config)
{
   struct config_file_entry entry;
   bool present;
   char key[96];
   char belonging[1024];

   menu->control_count = 0;
   belonging[0] = '\0';
   if (menu->profile_id[0])
   {
      snprintf(key, sizeof(key), "controls_variant_controls_%s",
            menu->profile_id);
      if (!config_get_array(config, key, belonging, sizeof(belonging)))
         belonging[0] = '\0';
   }
   for (present = config_get_entry_list_head(config, &entry); present;
        present = config_get_entry_list_next(&entry))
   {
      const char *id;
      unsigned bind_index;

      if (!entry.key || strncmp(entry.key, "rib_label_", 10))
         continue;
      id = entry.key + 10;
      if (!*id)
         continue;
      if (!rib_rmlui_control_belongs(belonging, id))
         continue;
      if (!rib_control_bind_index(id, &bind_index))
      {
         RARCH_WARN("[RIB] '%s' is not a libretro bind; the menu will not show "
               "it. Check the id against DECLARE_BIND in configuration.c.\n", id);
         continue;
      }
      if (menu->control_count >= RIB_CONTROL_MAX)
      {
         /* We log this, so that a control left out, such as a DualShock
          * stick, appears in the log. */
         RARCH_ERR("[RIB] more than %d controls declared; '%s' and anything "
               "after it are unreachable.\n", RIB_CONTROL_MAX, id);
         return;
      }
      strlcpy(menu->controls[menu->control_count].id, id,
            sizeof(menu->controls[menu->control_count].id));
      menu->controls[menu->control_count].bind_index = bind_index;
      ++menu->control_count;
   }
}

/* Read the screens declared in the design.
 *
 * `screens` is a space-separated list of ids that we write at export from the
 * declaration in the design, with the block of markup, the heading and the
 * footer hint for each. There are no screens in the player itself, so to add
 * one we change a design and not this file.
 */
static void rib_rmlui_discover_screens(const char *asset_directory)
{
   char path[PATH_MAX_LENGTH];
   config_file_t *config;
   char list[512];
   char *cursor;
   char *token;

   rib_rmlui_clear_screens();
   if (!asset_directory || !*asset_directory)
      return;
   snprintf(path, sizeof(path), "%s/design.cfg", asset_directory);
   if (!(config = config_file_new_from_path_to_string(path)))
   {
      RARCH_LOG("[RIB] no design declarations at %s; the menu has no screens "
            "and nothing will switch.\n", path);
      return;
   }
   if (!config_get_array(config, "screens", list, sizeof(list)))
   {
      config_file_free(config);
      return;
   }

   cursor = list;
   while ((token = strtok_r(cursor, " ", &cursor)))
   {
      char key[96];
      char panel[128];
      char heading[128];
      char footer[128];
      char button[128];

      if (!*token)
         continue;
      snprintf(key, sizeof(key), "screen_panel_%s", token);
      if (!config_get_array(config, key, panel, sizeof(panel)))
         continue;
      snprintf(key, sizeof(key), "screen_heading_%s", token);
      if (!config_get_array(config, key, heading, sizeof(heading)))
         heading[0] = '\0';
      snprintf(key, sizeof(key), "screen_footer_%s", token);
      if (!config_get_array(config, key, footer, sizeof(footer)))
         footer[0] = '\0';
      snprintf(key, sizeof(key), "screen_button_%s", token);
      if (!config_get_array(config, key, button, sizeof(button)))
         button[0] = '\0';
      rib_rmlui_declare_screen(token, panel, heading, footer, button);
   }
   config_file_free(config);
}

/* Read the controllers available for this console.
 *
 * `controls_variants` is a space-separated list of ids that we write at
 * export, with a display name for each. There are no controller names in the
 * player, for the same reason there are no control names: they are all in
 * the console package.
 */
static void rib_rmlui_discover_devices(rib_rmlui_menu_t *menu,
      config_file_t *config)
{
   char list[512];
   char *cursor;
   char *token;

   menu->device_count = 0;
   if (!config_get_array(config, "controls_variants", list, sizeof(list)))
      return;

   cursor = list;
   while ((token = strtok_r(cursor, " ", &cursor)))
   {
      char key[96];
      char name[NAME_MAX_LENGTH];

      if (!*token)
         continue;
      if (menu->device_count >= RIB_DEVICE_MAX)
      {
         RARCH_ERR("[RIB] more than %d controllers offered; '%s' and any after "
               "it cannot be chosen.\n", RIB_DEVICE_MAX, token);
         return;
      }
      strlcpy(menu->device_ids[menu->device_count], token,
            sizeof(menu->device_ids[menu->device_count]));
      snprintf(key, sizeof(key), "controls_variant_device_%s", token);
      menu->device_libretro[menu->device_count] = 0;
      {
         char device[32];
         if (config_get_array(config, key, device, sizeof(device)))
            menu->device_libretro[menu->device_count] =
               (unsigned)strtoul(device, NULL, 10);
      }
      snprintf(key, sizeof(key), "controls_variant_name_%s", token);
      if (config_get_array(config, key, name, sizeof(name)))
         strlcpy(menu->device_names[menu->device_count], name,
               sizeof(menu->device_names[menu->device_count]));
      else
         strlcpy(menu->device_names[menu->device_count], token,
               sizeof(menu->device_names[menu->device_count]));
      ++menu->device_count;
   }
}

int rib_rmlui_device_count(void)
{
   const rib_rmlui_menu_t *menu = rib_rmlui_active_menu;
   return menu ? menu->device_count : 0;
}

const char *rib_rmlui_device_id(int index)
{
   const rib_rmlui_menu_t *menu = rib_rmlui_active_menu;
   if (!menu || index < 0 || index >= menu->device_count)
      return NULL;
   return menu->device_ids[index];
}

const char *rib_rmlui_device_name(int index)
{
   const rib_rmlui_menu_t *menu = rib_rmlui_active_menu;
   if (!menu || index < 0 || index >= menu->device_count)
      return NULL;
   return menu->device_names[index];
}

/* We keep no control names in the bridge and read them from here. */
int rib_rmlui_control_capacity(void)
{
   return RIB_CONTROL_MAX;
}

const char *rib_rmlui_control_id(int index)
{
   const rib_rmlui_menu_t *menu = rib_rmlui_active_menu;
   if (!menu || index < 0 || index >= menu->control_count)
      return NULL;
   return menu->controls[index].id;
}

static bool rib_control_is_active(const rib_rmlui_menu_t *menu, int index)
{
   return menu && index >= 0 && index < menu->control_count &&
          menu->control_active[index];
}

static const char *rib_control_console_name(
      const rib_rmlui_menu_t *menu, int index)
{
   if (menu && menu->control_labels[index][0])
      return menu->control_labels[index];
   return menu->controls[index].id;
}

static int rib_control_first(const rib_rmlui_menu_t *menu)
{
   int index;
   for (index = 0; index < menu->control_count; ++index)
      if (rib_control_is_active(menu, index))
         return index;
   return 0;
}

static int rib_control_step(const rib_rmlui_menu_t *menu,
      int current, int direction)
{
   int sequence[RIB_CONTROL_MAX + 3];
   int sequence_count = 0;
   int position = 0;
   int index;

   /* The player moves through the controls in the order of their declaration.
    *
    * We declare the controls in each console package in the order the player
    * moves through them. On the Game Boy, B comes before A because that is
    * where the buttons are on the console. */
   for (index = 0; index < menu->control_count; ++index)
      if (rib_control_is_active(menu, index))
         sequence[sequence_count++] = index;
   sequence[sequence_count++] = RIB_CONTROL_MAX;
   sequence[sequence_count++] = RIB_CONTROL_MAX + 1;

   for (position = 0; position < sequence_count; ++position)
      if (sequence[position] == current)
         break;
   if (position == sequence_count)
      position = 0;
   else
      position = (position + direction + sequence_count) % sequence_count;
   return sequence[position];
}

static bool rib_rmlui_focus_is_slot(int focused)
{
   return focused >= RIB_RMLUI_ACTION_SELECT_SLOT_1 &&
          focused <= RIB_RMLUI_ACTION_SELECT_SLOT_6;
}

static int rib_rmlui_focus_slot(int focused)
{
   return focused - RIB_RMLUI_ACTION_SELECT_SLOT_1 + 1;
}

static void rib_rmlui_select_slot(rib_rmlui_menu_t *menu, int slot)
{
   if (!menu || slot < 1 || slot > 6)
      return;
   menu->selected_slot = slot;
   rib_rmlui_set_selected_slot(slot);
}

static bool rib_rmlui_load_is_available(const rib_rmlui_menu_t *menu);

static void rib_rmlui_focus(rib_rmlui_menu_t *menu, int focused,
      bool direction_up)
{
   bool changed;

   if (!menu)
      return;
   if (focused == RIB_RMLUI_ACTION_LOAD &&
       !rib_rmlui_load_is_available(menu))
      focused = direction_up ? RIB_RMLUI_ACTION_SAVE :
            RIB_RMLUI_ACTION_CONTROLS;
   changed = menu->focused != focused;
   menu->focused = focused;
   rib_rmlui_set_focused(focused);
   if (rib_rmlui_focus_is_slot(focused))
      rib_rmlui_select_slot(menu, rib_rmlui_focus_slot(focused));
#ifdef HAVE_AUDIOMIXER
   if (changed)
      audio_driver_mixer_play_scroll_sound(direction_up);
#endif
}

static bool rib_rmlui_load_is_available(const rib_rmlui_menu_t *menu)
{
   char state_path[PATH_MAX_LENGTH] = {0};

   return menu && runloop_get_savestate_path(state_path,
         sizeof(state_path), menu->selected_slot) && path_is_valid(state_path);
}

static void rib_rmlui_refresh_slots(void)
{
   runloop_state_t *runloop_st = runloop_state_get_ptr();
   int slot;

   for (slot = 1; slot <= 6; ++slot)
   {
      char state_path[PATH_MAX_LENGTH] = {0};
      char thumbnail_path[PATH_MAX_LENGTH] = {0};
      bool occupied = runloop_get_savestate_path(
            state_path, sizeof(state_path), slot) && path_is_valid(state_path);

      if (runloop_st && runloop_st->name.savestate[0])
         gfx_savestate_thumbnail_get_path(thumbnail_path,
               sizeof(thumbnail_path), runloop_st->name.savestate, slot);
      if (!path_is_valid(thumbnail_path))
         thumbnail_path[0] = '\0';

      rib_rmlui_set_game_aspect(video_driver_get_core_aspect());
      rib_rmlui_set_slot_state(slot, occupied, thumbnail_path);
   }
}

static bool rib_rmlui_begin_transfer(rib_rmlui_menu_t *menu, bool is_save)
{
   if (!menu || menu->transfer_pending)
      return false;

   menu->transfer_path[0] = '\0';
   if (!runloop_get_savestate_path(menu->transfer_path,
         sizeof(menu->transfer_path), menu->selected_slot))
      menu->transfer_path[0] = '\0';
   menu->transfer_is_save = is_save;
   menu->transfer_slot = menu->selected_slot;
   menu->transfer_pending = true;
   return true;
}

void rib_rmlui_notify_state_task(const char *path, int slot,
      bool is_save, bool success)
{
   rib_rmlui_menu_t *menu = rib_rmlui_active_menu;
   char status[64];

   if (!rib_rmlui_state_task_matches(
         menu && menu->transfer_pending,
         menu && menu->transfer_is_save,
         menu ? menu->transfer_path : NULL,
         menu ? menu->transfer_slot : -1,
         path, slot, is_save))
      return;

   menu->transfer_pending = false;
   if (success)
      snprintf(status, sizeof(status),
            is_save ? "SLOT %d SAVED" : "SLOT %d LOADED",
            menu->transfer_slot);
   else
      snprintf(status, sizeof(status),
            is_save ? "SAVE FAILED" : "LOAD FAILED");
   rib_rmlui_set_status(status);
}

static void rib_rmlui_apply_control_bind(config_file_t *config,
      const rib_control_t *control)
{
   char base[64];
   struct config_entry_list *entry;
   struct retro_keybind *bind;

   if (!config || !control)
      return;

   bind = &input_config_binds[0][control->bind_index];
   snprintf(base, sizeof(base), "input_player1_%s", control->id);
   entry = config_get_entry(config, base);
   if (entry && entry->value && *entry->value)
   {
      input_keyboard_mapping_bits(0, bind->key);
      bind->key = input_config_translate_str_to_rk(
            entry->value, strlen(entry->value));
      input_keyboard_mapping_bits(1, bind->key);
   }
   input_config_parse_joy_button(base, config, "input_player1",
         control->id, bind);
   input_config_parse_joy_axis(base, config, "input_player1",
         control->id, bind);
   input_config_parse_mouse_button(base, config, "input_player1",
         control->id, bind);
}

static void rib_rmlui_restore_keyboard_mapping_bits(void)
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

static bool rib_rmlui_load_controls_file(rib_rmlui_menu_t *menu,
      const char *path, bool defaults)
{
   config_file_t *config;
   char profile[32] = {0};
   int index;

   if (!menu || !path || !(config = config_file_new_from_path_to_string(path)))
      return false;

   /* The starting pad is in the author's defaults. We load the per-game
    * override after them. It contains the pad the player chose later, so we
    * use that pad, and the player sees it in the picker at every launch. We
    * read the list of variants only from the defaults, because the override
    * does not contain it. */
   if (config_get_array(config, "controls_profile", profile, sizeof(profile))
         && profile[0])
      strlcpy(menu->profile_id, profile, sizeof(menu->profile_id));

   if (defaults)
   {
      rib_rmlui_discover_controls(menu, config);
      rib_rmlui_discover_devices(menu, config);
      /* We load the document before we build these lists, so there are no
       * listeners yet on its control and picker elements. */
      rib_rmlui_wire_controls();
      rib_rmlui_wire_device_picker();
      rib_rmlui_set_device_picker(false, menu->profile_id);
   }
   else if (profile[0])
      rib_rmlui_set_device_picker(false, menu->profile_id);

   if (defaults)
      for (index = 0; index < menu->control_count; ++index)
      {
         char key[96];
         const char *suffixes[] = {"", "_btn", "_axis", "_mbtn"};
         unsigned suffix_index;
         menu->control_active[index] = false;
         for (suffix_index = 0; suffix_index < ARRAY_SIZE(suffixes);
              ++suffix_index)
         {
            snprintf(key, sizeof(key), "input_player1_%s%s",
                  menu->controls[index].id, suffixes[suffix_index]);
            if (config_get_entry(config, key))
            {
               menu->control_active[index] = true;
               break;
            }
         }
      }

   for (index = 0; index < menu->control_count; ++index)
   {
      char key[64];
      char label[NAME_MAX_LENGTH] = {0};

      if (!rib_control_is_active(menu, index))
         continue;
      if (defaults)
      {
         struct retro_keybind *bind =
            &input_config_binds[0][menu->controls[index].bind_index];
         menu->control_labels[index][0] = '\0';
         input_keyboard_mapping_bits(0, bind->key);
         bind->key = RETROK_UNKNOWN;
         bind->joykey = NO_BTN;
         bind->joyaxis = AXIS_NONE;
         bind->mbutton = NO_BTN;
      }
      snprintf(key, sizeof(key), "rib_label_%s", menu->controls[index].id);
      if (config_get_array(config, key, label, sizeof(label)))
         strlcpy(menu->control_labels[index], label,
               sizeof(menu->control_labels[index]));
      rib_rmlui_apply_control_bind(config, &menu->controls[index]);
      if (defaults)
      {
         strlcpy(menu->default_labels[index], menu->control_labels[index],
               sizeof(menu->default_labels[index]));
         menu->default_binds[index] =
            input_config_binds[0][menu->controls[index].bind_index];
      }
   }
   rib_rmlui_restore_keyboard_mapping_bits();
   config_file_free(config);
   return true;
}

/* Read the controls for the pad in use again, and attach the listeners again.
 *
 * We call this when the player chooses another controller in the picker. We
 * replaced the elements of the scene, so their listeners are gone, and the
 * new pad has a different set of controls. The export contains the labels of
 * every pad in the picker and the ids of each pad, so we only read them again
 * and ask for nothing.
 */
static void rib_rmlui_reload_controls(rib_rmlui_menu_t *menu)
{
   const char *asset_directory = getenv("ROMINABOX_RML_ASSETS");
   char defaults_path[PATH_MAX_LENGTH];

   if (!asset_directory || !*asset_directory)
      asset_directory = RIB_RMLUI_DEFAULT_ASSETS;
   snprintf(defaults_path, sizeof(defaults_path),
         "%s/controls-defaults.cfg", asset_directory);

   menu->control_focus = 0;
   menu->selected_control = 0;
   if (!rib_rmlui_load_controls_file(menu, defaults_path, true))
   {
      RARCH_ERR("[RIB] could not re-read controls from %s after changing "
            "controller; the menu still lists the previous pad.\n",
            defaults_path);
      return;
   }
   if (menu->controls_path[0])
      rib_rmlui_load_controls_file(menu, menu->controls_path, false);
   rib_rmlui_wire_controls();
   menu->control_focus = rib_control_first(menu);
   menu->selected_control = menu->control_focus;
   rib_rmlui_refresh_controls(menu);
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

static bool rib_rmlui_save_controls(rib_rmlui_menu_t *menu)
{
   config_file_t *config;
   char temporary_path[PATH_MAX_LENGTH];
   int index;

   if (!menu || !menu->controls_path[0] || !(config = config_file_new_alloc()))
      return false;

   config_set_string(config, "controls_profile", menu->profile_id);
   for (index = 0; index < menu->control_count; ++index)
   {
      const struct retro_keybind *bind;
      char key[96];
      char value[64];

      if (!rib_control_is_active(menu, index))
         continue;
      bind = &input_config_binds[0][menu->controls[index].bind_index];
      snprintf(key, sizeof(key), "rib_label_%s", menu->controls[index].id);
      config_set_string(config, key, menu->control_labels[index]);

      snprintf(key, sizeof(key), "input_player1_%s", menu->controls[index].id);
      input_keymaps_translate_rk_to_str(bind->key, value, sizeof(value));
      config_set_string(config, key, value);
      snprintf(key, sizeof(key), "input_player1_%s_btn", menu->controls[index].id);
      rib_rmlui_save_joy_button(config, key, bind->joykey);
      snprintf(key, sizeof(key), "input_player1_%s_axis", menu->controls[index].id);
      rib_rmlui_save_axis(config, key, bind->joyaxis);
      snprintf(key, sizeof(key), "input_player1_%s_mbtn", menu->controls[index].id);
      rib_rmlui_save_mouse_button(config, key, bind->mbutton);
   }

   snprintf(temporary_path, sizeof(temporary_path), "%s.tmp",
         menu->controls_path);
   if (!config_file_write(config, temporary_path, true) ||
       rename(temporary_path, menu->controls_path) != 0)
   {
      config_file_free(config);
      return false;
   }
   config_file_free(config);
   return true;
}

static void rib_rmlui_refresh_controls(rib_rmlui_menu_t *menu)
{
   settings_t *settings = config_get_ptr();
   int index;
   for (index = 0; menu && index < menu->control_count; ++index)
   {
      char display_label[NAME_MAX_LENGTH * 2];
      char binding[NAME_MAX_LENGTH] = {0};

      if (!rib_control_is_active(menu, index))
         continue;
      strlcpy(display_label, menu->control_labels[index],
            sizeof(display_label));
      input_config_get_bind_string(settings, binding,
            &input_config_binds[0][menu->controls[index].bind_index],
            NULL, sizeof(binding));
      rib_rmlui_set_control_state(menu->controls[index].id,
            display_label, binding,
            menu->controls_visible && menu->control_focus == index,
            menu->capture_active && menu->capture_control == index);
   }
   rib_rmlui_set_controls_action_focus(
         menu && menu->controls_visible && menu->control_focus == RIB_CONTROL_MAX,
         menu && menu->controls_visible && menu->control_focus == RIB_CONTROL_MAX + 1,
         menu && menu->capture_active);
}

static void rib_rmlui_reset_interaction(rib_rmlui_menu_t *menu, bool opening)
{
   if (!menu)
      return;
   if (menu->capture_active)
      rib_rmlui_cancel_capture(menu, NULL);
   menu->controls_visible = false;
   menu->pointer_pressed = false;
   menu->capture_ignore_pointer = false;
   menu->focused = RIB_RMLUI_ACTION_RESUME;
   rib_rmlui_clear_intents();
   rib_rmlui_pointer_leave();
   if (opening)
   {
      rib_rmlui_show_controls(false);
      rib_rmlui_set_focused(menu->focused);
      rib_rmlui_set_footer_hint("ESC  CONTINUE");
   }
}

static void rib_rmlui_toggle(void *userdata, bool on)
{
   rib_rmlui_reset_interaction((rib_rmlui_menu_t*)userdata, on);
}

bool rib_rmlui_consume_menu_toggle(void *userdata)
{
   rib_rmlui_menu_t *menu = (rib_rmlui_menu_t*)userdata;
   return menu && rib_rmlui_toggle_stays_in_menu(
         menu->controls_visible, menu->capture_active);
}

static void rib_rmlui_cancel_capture(rib_rmlui_menu_t *menu,
      const char *status)
{
   if (!menu || !menu->capture_active)
      return;
   menu_input_rib_bind_cancel();
   menu->capture_active = false;
   rib_rmlui_set_controls_status(status ? status : "BINDING UNCHANGED");
   rib_rmlui_set_footer_hint(menu->controls_visible ? "ESC  BACK" :
                                                       "ESC  CONTINUE");
   rib_rmlui_refresh_controls(menu);
}

static void rib_rmlui_start_capture(rib_rmlui_menu_t *menu, int index)
{
   char status[96];
   if (!menu || !rib_control_is_active(menu, index))
      return;
   if (!menu_input_rib_bind_start(menu->controls[index].bind_index,
            RIB_CONTROL_CAPTURE_SECONDS))
   {
      rib_rmlui_set_controls_status("CAPTURE COULD NOT START");
      return;
   }
   menu->capture_active = true;
   menu->capture_control = index;
   menu->capture_ignore_pointer = true;
   snprintf(status, sizeof(status), "%s: PRESS AN INPUT (10)",
         rib_control_console_name(menu, index));
   rib_rmlui_set_controls_status(status);
   rib_rmlui_set_footer_hint("ESC  CANCEL");
   rib_rmlui_refresh_controls(menu);
}

static int rib_rmlui_find_binding_conflict(
      const rib_rmlui_menu_t *menu, int changed_index)
{
   const struct retro_keybind *changed;
   int index;
   if (!rib_control_is_active(menu, changed_index))
      return -1;
   changed = &input_config_binds[0][menu->controls[changed_index].bind_index];
   for (index = 0; index < menu->control_count; ++index)
   {
      const struct retro_keybind *candidate;
      if (index == changed_index || !rib_control_is_active(menu, index))
         continue;
      candidate = &input_config_binds[0][menu->controls[index].bind_index];
      if ((changed->key != RETROK_UNKNOWN && changed->key == candidate->key) ||
          (changed->joykey != NO_BTN && changed->joykey == candidate->joykey) ||
          (changed->joyaxis != AXIS_NONE && changed->joyaxis == candidate->joyaxis) ||
          (changed->mbutton != NO_BTN && changed->mbutton == candidate->mbutton))
         return index;
   }
   return -1;
}

static void rib_rmlui_play_action_sound(int action)
{
#ifdef HAVE_AUDIOMIXER
   switch (rib_rmlui_action_sound(action))
   {
      case RIB_MENU_SOUND_OK:
         audio_driver_mixer_play_menu_sound(AUDIO_MIXER_SYSTEM_SLOT_OK);
         break;
      case RIB_MENU_SOUND_CANCEL:
         audio_driver_mixer_play_menu_sound(AUDIO_MIXER_SYSTEM_SLOT_CANCEL);
         break;
      case RIB_MENU_SOUND_NONE:
         break;
   }
#endif
}

/* The path of the core remap file, the one in use when there is no game or
 * content-directory remap (config_load_remap):
 * <input_remapping_directory>/<library name>/<library name>.rmp
 * With sort-by-controller on, we add the name of the physical pad, because
 * the path in use with that setting contains it. */
static bool rib_rmlui_core_remap_path(char *path, size_t len)
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

/* Update input_libretro_device_p1 in an existing remap, or create one.
 *
 * That key is valid only in a remap file. If we replaced the whole file, as
 * with input_remapping_save_file, we would also write turbo, port and analog
 * settings the author never put there, so we keep the other keys as they are.
 * We update the file in place and never remove it. At launch we copy the
 * author's remap into the data directory only when that file is missing, so
 * without it the next launch would use the author's device again. */
static bool rib_rmlui_write_remap_device(const char *path, unsigned device)
{
   config_file_t *conf;
   char directory[PATH_MAX_LENGTH];
   char temporary[PATH_MAX_LENGTH];
   char existing[32];
   char wanted[32];
   bool existed;

   if (!path || !*path || !device)
      return false;

   existed = path_is_valid(path);
   conf = existed ? config_file_new_from_path_to_string(path) : NULL;
   if (existed && !conf)
      return false;
   if (!conf && !(conf = config_file_new_alloc()))
      return false;

   snprintf(wanted, sizeof(wanted), "%u", device);
   if (config_get_array(conf, "input_libretro_device_p1",
            existing, sizeof(existing))
         && string_is_equal(existing, wanted))
   {
      config_file_free(conf);
      return true;
   }
   config_set_uint(conf, "input_libretro_device_p1", device);

   fill_pathname_parent_dir(directory, path, sizeof(directory));
   if (*directory && !path_is_directory(directory) && !path_mkdir(directory))
   {
      config_file_free(conf);
      return false;
   }

   if (strlcpy(temporary, path, sizeof(temporary)) >= sizeof(temporary)
         || strlcat(temporary, ".tmp", sizeof(temporary)) >= sizeof(temporary))
   {
      config_file_free(conf);
      return false;
   }

   if (!config_file_write(conf, temporary, true))
   {
      config_file_free(conf);
      return false;
   }
   config_file_free(conf);

   /* On POSIX rename() replaces an existing file, and on Windows it fails. */
#if defined(_WIN32)
   if (filestream_exists(path))
      filestream_delete(path);
#endif
   if (filestream_rename(temporary, path) != 0)
   {
      filestream_delete(temporary);
      return false;
   }
   return true;
}

static bool rib_rmlui_persist_libretro_device(unsigned device)
{
   char core_path[PATH_MAX_LENGTH];
   const char *active;
   bool ok;

   if (!rib_rmlui_core_remap_path(core_path, sizeof(core_path)))
      return false;

   ok = rib_rmlui_write_remap_device(core_path, device);
   /* A game or content-directory remap, when there is one, comes before the
    * core file and would hide it, so we write the same device into it too. */
   active = runloop_state_get_ptr()->name.remapfile;
   if (active && *active && !string_is_equal(active, core_path))
      ok = rib_rmlui_write_remap_device(active, device) && ok;
   return ok;
}

static bool rib_save_volume(const rib_rmlui_menu_t *menu, float db)
{
   char temporary[PATH_MAX_LENGTH];
   FILE *file;

   if (!menu || !menu->volume_path[0])
      return false;
   snprintf(temporary, sizeof(temporary), "%s.tmp", menu->volume_path);
   if (!(file = fopen(temporary, "w")))
      return false;
   fprintf(file, "%s = \"%.1f\"\n", RIB_VOLUME_KEY, db);
   if (fclose(file) != 0)
   {
      filestream_delete(temporary);
      return false;
   }
#if defined(_WIN32)
   if (filestream_exists(menu->volume_path))
      filestream_delete(menu->volume_path);
#endif
   if (rename(temporary, menu->volume_path) != 0)
   {
      filestream_delete(temporary);
      return false;
   }
   return true;
}

static void rib_paint_volume(void)
{
   settings_t *settings = config_get_ptr();
   float db = settings ? settings->floats.audio_volume : AUDIO_VOLUME_DEFAULT_DB;

   db = rib_volume_quantize_db(db);
   /* No readout. Low and high are in the design, and the position of the
    * thumb is the value. With an empty string we clear what we wrote before. */
   rib_rmlui_set_slider(RIB_VOLUME_SLIDER_ID,
         rib_volume_fraction_from_db(db), "");
}

static void rib_set_volume_db(rib_rmlui_menu_t *menu, float db, bool persist)
{
   settings_t *settings = config_get_ptr();
   bool *muted_flag = audio_get_bool_ptr(AUDIO_ACTION_MUTE_ENABLE);

   db = rib_volume_quantize_db(db);
   /* There is no control for mute, but a file or a hotkey may have set it.
    * The player chooses only the level, so we turn mute off. */
   if (muted_flag)
      *muted_flag = false;
   if (settings)
      configuration_set_float(settings, settings->floats.audio_volume, db);
   audio_set_float(AUDIO_ACTION_VOLUME_GAIN, db);
   if (persist)
      rib_save_volume(menu, db);
   rib_paint_volume();
}

static void rib_rmlui_perform_action(rib_rmlui_menu_t *menu, int action)
{
   settings_t *settings = config_get_ptr();
   char status[64];
   int control_index;

   if (!menu)
      return;

   if (menu->capture_active &&
       action != RIB_RMLUI_ACTION_CONTROLS_CANCEL &&
       action != RIB_RMLUI_ACTION_CONTROLS_BACK)
      return;

   if (action == RIB_RMLUI_ACTION_DEVICE_PICKER_TOGGLE)
   {
      menu->device_picker_open = !menu->device_picker_open;
      rib_rmlui_set_device_picker(menu->device_picker_open, menu->profile_id);
      return;
   }
   if (action == RIB_RMLUI_ACTION_SLIDER)
   {
      rib_rmlui_play_action_sound(action);
      if (string_is_equal(rib_rmlui_changed_part(), RIB_VOLUME_SLIDER_ID))
         rib_set_volume_db(menu,
               rib_volume_db_from_fraction(rib_rmlui_changed_fraction()), true);
      return;
   }
   if (action == RIB_RMLUI_ACTION_TOGGLE)
   {
      rib_rmlui_play_action_sound(action);
      return;
   }
   if (action == RIB_RMLUI_ACTION_SHOW_SCREEN)
   {
      /* We pass the screen next to the action, so declaring a screen never
       * adds to the enum. We still record whether the controls screen is
       * open, because capture and navigation work differently there. */
      const char *wanted = rib_rmlui_requested_screen();
      if (wanted && *wanted && rib_rmlui_show_screen(wanted))
      {
         rib_rmlui_play_action_sound(action);
         strlcpy(menu->screen, wanted, sizeof(menu->screen));
         /* The footer and the heading are in the design, with the screen.
          * Here we keep only the case of the controls screen, where capture
          * and navigation work differently. For any other screen there is
          * nothing to add here. */
         menu->controls_visible = string_is_equal(wanted, "controls");
         if (menu->controls_visible)
         {
            menu->control_focus = rib_control_first(menu);
            menu->selected_control = menu->control_focus;
            rib_rmlui_set_controls_status("SELECT A CONTROL TO REBIND");
            rib_rmlui_refresh_controls(menu);
         }
         else if (menu->capture_active)
            rib_rmlui_cancel_capture(menu, "BINDING UNCHANGED");
         if (!menu->controls_visible && !string_is_equal(wanted, "pause"))
         {
            char ids[16][64];
            const char *panel = rib_rmlui_screen_panel(menu->screen);
            if (rib_rmlui_focusables(panel, ids, 16) > 0)
            {
               menu->panel_focus = 0;
               rib_rmlui_mark_focused(panel, ids[0]);
            }
         }
         /* We measure the slider from the box of its track. While the panel
          * is hidden that width is zero, so a paint leaves the thumb where
          * the stylesheet put it, at the quiet end. Paint it again now that
          * the screen is shown. */
         rib_paint_volume();
      }
      return;
   }

   if (action == RIB_RMLUI_ACTION_DEVICE_PICKER_CHOOSE)
   {
      /* We pass the chosen id next to the action, not inside it, so the number
       * of controllers for a console never has to be part of an enum. */
      const char *chosen = rib_rmlui_chosen_device();
      menu->device_picker_open = false;
      if (chosen && *chosen && !string_is_equal(chosen, menu->profile_id))
      {
         int index;
         unsigned device = 0;
         bool known = false;

         strlcpy(menu->profile_id, chosen, sizeof(menu->profile_id));
         /* The pad belongs to the player who picks it, so we write it to the
          * per-game override and never to the author's fixed defaults. */
         rib_rmlui_save_controls(menu);

         for (index = 0; index < menu->device_count; ++index)
            if (string_is_equal(menu->device_ids[index], chosen))
            {
               device = menu->device_libretro[index];
               known = true;
               break;
            }

         /* We apply it to the core now, not at the next launch. The
          * emulated device is a setting in RetroArch, and we apply it again
          * with CMD_EVENT_CONTROLLER_INIT, as for any other change of device
          * in the frontend. A catalog device of 0 means "no subclass". The
          * default in the frontend is then a joypad, and writing 0 would
          * connect nothing. At the next launch we read the device from the
          * remap and not from the per-game override. */
         if (known && settings)
         {
            unsigned applied = device ? device : (unsigned)RETRO_DEVICE_JOYPAD;
            configuration_set_uint(settings,
                  settings->uints.input_libretro_device[0], applied);
            command_event(CMD_EVENT_CONTROLLER_INIT, NULL);
            if (!rib_rmlui_persist_libretro_device(applied))
               RARCH_ERR("[RIB] controller '%s' is active as device %u, but "
                     "the remap could not be written. The next launch will "
                     "restore the previous device.\n", chosen, applied);
            else
               RARCH_LOG("[RIB] controller '%s' applied as device %u.\n",
                     chosen, applied);
         }

         /* Draw the chosen pad. The export contains a scene for each pad in
          * the picker, so we have the art and the positions here. */
         {
            const char *assets = getenv("ROMINABOX_RML_ASSETS");
            char path[PATH_MAX_LENGTH];
            if (!assets || !*assets)
               assets = RIB_RMLUI_DEFAULT_ASSETS;
            snprintf(path, sizeof(path), "%s/scene-%s.rml", assets, chosen);
            {
               int64_t length = 0;
               void *markup = NULL;
               if (filestream_read_file(path, &markup, &length) && markup)
               {
                  if (rib_rmlui_set_scene((const char*)markup))
                  {
                     /* We replaced the elements of the old scene, which had
                      * the listeners, and this pad has a different set of
                      * controls. */
                     rib_rmlui_reload_controls(menu);
                     RARCH_LOG("[RIB] drawing '%s' from %s.\n", chosen, path);
                  }
                  free(markup);
               }
               else
                  RARCH_ERR("[RIB] no scene for '%s' at %s; the pad on screen "
                        "is still the one the game was exported with.\n",
                        chosen, path);
            }
         }
      }
      rib_rmlui_set_device_picker(false, menu->profile_id);
      return;
   }

   if (action >= RIB_RMLUI_ACTION_CONTROL_FIRST &&
       action <= RIB_RMLUI_ACTION_CONTROL_LAST)
   {
      control_index = action - RIB_RMLUI_ACTION_CONTROL_FIRST;
      if (rib_control_is_active(menu, control_index))
      {
         rib_rmlui_play_action_sound(action);
         menu->control_focus = control_index;
         menu->selected_control = control_index;
         rib_rmlui_refresh_controls(menu);
         rib_rmlui_start_capture(menu, control_index);
      }
      return;
   }

   if ((action == RIB_RMLUI_ACTION_SAVE ||
            action == RIB_RMLUI_ACTION_LOAD) &&
         menu->transfer_pending)
      return;

   if (action == RIB_RMLUI_ACTION_LOAD && !rib_rmlui_load_is_available(menu))
      return;
   rib_rmlui_play_action_sound(action);

   switch (action)
   {
      case RIB_RMLUI_ACTION_SAVE:
         if (!rib_rmlui_begin_transfer(menu, true))
            return;
         if (settings)
            configuration_set_int(settings, settings->ints.state_slot,
                  menu->selected_slot);
         snprintf(status, sizeof(status), "SAVING SLOT %d...",
               menu->selected_slot);
         rib_rmlui_set_status(status);
         if (!command_event(CMD_EVENT_SAVE_STATE, NULL) &&
               menu->transfer_pending)
            rib_rmlui_notify_state_task(menu->transfer_path,
                  menu->transfer_slot, true, false);
         break;
      case RIB_RMLUI_ACTION_LOAD:
         if (!rib_rmlui_load_is_actionable(rib_rmlui_load_is_available(menu)))
            return;
         if (!rib_rmlui_begin_transfer(menu, false))
            return;
         if (settings)
            configuration_set_int(settings, settings->ints.state_slot,
                  menu->selected_slot);
         snprintf(status, sizeof(status), "LOADING SLOT %d...",
               menu->selected_slot);
         rib_rmlui_set_status(status);
         if (!command_event(CMD_EVENT_LOAD_STATE, NULL) &&
               menu->transfer_pending)
            rib_rmlui_notify_state_task(menu->transfer_path,
                  menu->transfer_slot, false, false);
         break;
      case RIB_RMLUI_ACTION_CONTROLS:
         menu->controls_visible = true;
         strlcpy(menu->screen, "controls", sizeof(menu->screen));
         menu->control_focus = rib_control_first(menu);
         menu->selected_control = menu->control_focus;
         /* The heading and the footer are in the design, with the
          * screen. */
         rib_rmlui_show_screen("controls");
         rib_rmlui_set_controls_status("SELECT A CONTROL TO REBIND");
         rib_rmlui_refresh_controls(menu);
         break;
      case RIB_RMLUI_ACTION_CONTROLS_BACK:
         if (menu->capture_active)
            rib_rmlui_cancel_capture(menu, "BINDING UNCHANGED");
         menu->controls_visible = false;
         menu->focused = RIB_RMLUI_ACTION_CONTROLS;
         strlcpy(menu->screen, "pause", sizeof(menu->screen));
         rib_rmlui_show_screen("pause");
         rib_rmlui_set_focused(menu->focused);
         break;
      case RIB_RMLUI_ACTION_CONTROLS_CANCEL:
         rib_rmlui_cancel_capture(menu, "BINDING UNCHANGED");
         break;
      case RIB_RMLUI_ACTION_CONTROLS_RESET:
         {
            char defaults_path[PATH_MAX_LENGTH];
            const char *asset_directory = getenv("ROMINABOX_RML_ASSETS");
            if (!asset_directory || !*asset_directory)
               asset_directory = RIB_RMLUI_DEFAULT_ASSETS;
            if (menu->capture_active)
               rib_rmlui_cancel_capture(menu, NULL);
            snprintf(defaults_path, sizeof(defaults_path),
                  "%s/controls-defaults.cfg", asset_directory);
            if (!rib_rmlui_load_controls_file(menu, defaults_path, true))
               rib_rmlui_set_controls_status("DEFAULTS COULD NOT BE LOADED");
            else if (!rib_rmlui_save_controls(menu))
               rib_rmlui_set_controls_status("DEFAULTS RESTORED; SAVE FAILED");
            else
               rib_rmlui_set_controls_status("DEFAULTS RESTORED");
            rib_rmlui_refresh_controls(menu);
         }
         break;
      case RIB_RMLUI_ACTION_RESUME:
         command_event(CMD_EVENT_MENU_TOGGLE, NULL);
         break;
      case RIB_RMLUI_ACTION_QUIT:
         command_event(CMD_EVENT_QUIT, NULL);
         break;
      case RIB_RMLUI_ACTION_SELECT_SLOT_1:
      case RIB_RMLUI_ACTION_SELECT_SLOT_2:
      case RIB_RMLUI_ACTION_SELECT_SLOT_3:
      case RIB_RMLUI_ACTION_SELECT_SLOT_4:
      case RIB_RMLUI_ACTION_SELECT_SLOT_5:
      case RIB_RMLUI_ACTION_SELECT_SLOT_6:
         rib_rmlui_focus(menu, action, false);
         break;
      default:
         break;
   }
}

static void *rib_rmlui_menu_init(void **userdata, bool video_is_threaded)
{
   menu_handle_t *menu_handle = (menu_handle_t*)calloc(1, sizeof(*menu_handle));
   rib_rmlui_menu_t *menu = (rib_rmlui_menu_t*)calloc(1, sizeof(*menu));
   (void)video_is_threaded;

   if (!menu_handle || !menu)
   {
      free(menu_handle);
      free(menu);
      return NULL;
   }

   menu->selected_slot = 1;
   menu->focused = RIB_RMLUI_ACTION_RESUME;
   strlcpy(menu->screen, "pause", sizeof(menu->screen));
   rib_rmlui_active_menu = menu;
   *userdata = menu;
   return menu_handle;
}

static void rib_rmlui_free(void *data)
{
   rib_rmlui_menu_t *menu = (rib_rmlui_menu_t*)data;
   if (rib_rmlui_active_menu == data)
      rib_rmlui_active_menu = NULL;
   if (menu && menu->capture_active)
      rib_rmlui_cancel_capture(menu, NULL);
   rib_rmlui_shutdown();
   rib_splash_active = false;
   /* The call that frees userdata is in menu_driver_ctl, after this callback. */
}

static void rib_rmlui_context_destroy(void *data)
{
   rib_rmlui_menu_t *menu = (rib_rmlui_menu_t*)data;
   if (menu && menu->capture_active)
      rib_rmlui_cancel_capture(menu, NULL);
   rib_rmlui_shutdown();
   if (menu)
      menu->initialized = false;
}

static void rib_rmlui_context_reset(void *data, bool video_is_threaded)
{
   rib_rmlui_menu_t *menu = (rib_rmlui_menu_t*)data;
   (void)video_is_threaded;
   if (menu)
      menu->initialized = false;
}

/* Drive the menu from ROMINABOX_MENU_SCRIPT, one element per frame.
 *
 * We take a screenshot of the menu from the menu itself. With offscreen
 * rendering we can show a state once its classes are set, but not that
 * pressing CONTROLS opens the controls screen, because the bridge code for
 * that is not loaded there. Here we click the same elements as a player,
 * through the same listeners, so with `--max-frames-ss` we capture a frame
 * of an actual state, and RetroArch exits by itself afterwards.
 *
 * The script is a comma-separated list of element ids, for example
 * "controls,controls-device-current". We click one per frame, so there is
 * a frame for the menu to update before the next click.
 *
 * We stop the run at an id that is not in the document. A screenshot taken
 * after clicking nothing would show the wrong thing, which is worse than no
 * picture.
 */
/* How many frames we wait after the last click before the screenshot. We
 * handle the queued click in the bridge one frame later, and we lay out a
 * picker that has just opened in the frame after that. */
#define RIB_SCRIPT_SETTLE_FRAMES 8

static void rib_rmlui_script_shot(void)
{
   const char *path       = getenv("ROMINABOX_MENU_SHOT");
   settings_t *settings   = config_get_ptr();
   runloop_state_t *state = runloop_state_get_ptr();
   video_driver_state_t *video_st = video_state_get_ptr();

   if (!path || !*path || !state || !video_st)
   {
      /* Without a screenshot, a script only drives the menu, so we leave the
       * game running and do not quit while someone may be playing it. */
      return;
   }

   /* We read the screenshot from the viewport and not from the framebuffer of
    * the core, because we draw the menu over the game and the framebuffer
    * contains only the game. We change the setting here, so there is no need
    * for a config override in the harness to get a picture of the menu. */
   if (settings)
      configuration_set_bool(settings, settings->bools.video_gpu_screenshot, true);

   /* We take the picture in the menu renderer, because the pixels are there:
    * the frame of the core with the menu drawn over it, still in the back
    * buffer. In RetroArch the screenshot code is in the runloop, after the
    * buffer is presented, and a viewport read at that point returns an empty
    * buffer, so the result is a black picture written without any error.
    *
    * We then end the run in the usual way, so no window stays open. */
   rib_rmlui_capture_next(path);
   state->max_frames = (unsigned)video_st->frame_count + 2;
   RARCH_LOG("[RIB] menu script shooting %s, exiting after frame %u.\n",
         path, state->max_frames);
}

static void rib_rmlui_run_script(void)
{
   static const char *script = NULL;
   static size_t at          = 0;
   static bool started       = false;
   static int settle         = RIB_SCRIPT_SETTLE_FRAMES;
   char id[128];
   const char *comma;
   size_t length;

   if (!started)
   {
      script  = getenv("ROMINABOX_MENU_SCRIPT");
      started = true;
      if (script)
         RARCH_LOG("[RIB] menu script: %s\n", *script ? script : "(none)");
   }
   if (!script)
      return;

   if (at >= strlen(script))
   {
      /* We have made every click. Wait for the menu to settle, take the
       * screenshot and let RetroArch exit by itself, so no window stays open. */
      if (settle-- <= 0)
      {
         settle = INT_MAX;
         rib_rmlui_script_shot();
      }
      return;
   }

   comma  = strchr(script + at, ',');
   length = comma ? (size_t)(comma - (script + at)) : strlen(script + at);
   if (length >= sizeof(id))
      length = sizeof(id) - 1;
   memcpy(id, script + at, length);
   id[length] = '\0';
   at += length + (comma ? 1 : 0);

   {
      char *at = strchr(id, '@');
      if (at)
      {
         *at = '\0';
         if (!rib_rmlui_commit_slider(id, (float)strtof(at + 1, NULL)))
         {
            RARCH_ERR("[RIB] menu script names no slider '%s'; stopping so no "
                  "screenshot is taken of the wrong screen.\n", id);
            command_event(CMD_EVENT_QUIT, NULL);
         }
         return;
      }
   }

   if (!rib_rmlui_click_element(id))
   {
      RARCH_ERR("[RIB] menu script names no element '%s'; stopping so no "
            "screenshot is taken of the wrong screen.\n", id);
      command_event(CMD_EVENT_QUIT, NULL);
      return;
   }
   RARCH_LOG("[RIB] menu script clicked '%s'.\n", id);
}

static void rib_rmlui_frame(void *data, video_frame_info_t *video_info)
{
   rib_rmlui_menu_t *menu = (rib_rmlui_menu_t*)data;
   menu_input_pointer_t pointer;
   const char *asset_directory = getenv("ROMINABOX_RML_ASSETS");
   const char *data_directory = getenv("ROMINABOX_DATA_DIR");

   if (!menu || !video_info)
      return;

   if (!menu->initialized)
   {
      if (!asset_directory || !*asset_directory)
         asset_directory = RIB_RMLUI_DEFAULT_ASSETS;
      menu->initialized = rib_rmlui_init(asset_directory,
            (int)video_info->width, (int)video_info->height);
      if (!menu->initialized)
      {
         RARCH_ERR("[RmlUi] Failed to initialize menu from %s.\n",
               asset_directory);
         rib_splash_active = false;
         if (!rib_splash_keep_menu_open)
            retroarch_menu_running_finished(false);
         return;
      }
      /* Read the screens in the design before we show any of them. */
      rib_rmlui_discover_screens(asset_directory);
      rib_rmlui_set_selected_slot(menu->selected_slot);
      rib_rmlui_set_focused(menu->focused);
      rib_rmlui_refresh_slots();
      rib_rmlui_show_controls(false);
      rib_rmlui_set_footer_hint("ESC  CONTINUE");
      if (!menu->controls_loaded)
      {
         char defaults_path[PATH_MAX_LENGTH];
         snprintf(defaults_path, sizeof(defaults_path),
               "%s/controls-defaults.cfg", asset_directory);
         if (!rib_rmlui_load_controls_file(menu, defaults_path, true))
            RARCH_WARN("[RmlUi] Controls defaults not found at %s.\n",
                  defaults_path);
         if (data_directory && *data_directory)
         {
            snprintf(menu->controls_path, sizeof(menu->controls_path),
                  "%s/controls.cfg", data_directory);
            snprintf(menu->volume_path, sizeof(menu->volume_path),
                  "%s/%s", data_directory, RIB_VOLUME_FILE);
            rib_rmlui_load_controls_file(menu, menu->controls_path, false);
         }
         menu->controls_loaded = true;
      }
      menu->control_focus = rib_control_first(menu);
      menu->selected_control = menu->control_focus;
      rib_rmlui_refresh_controls(menu);
      rib_rmlui_set_slider_step(RIB_VOLUME_SLIDER_ID,
            AUDIO_VOLUME_STEP_DB
            / (AUDIO_VOLUME_MAX_DB - AUDIO_VOLUME_MIN_DB));
      /* A file may have mute on, or a level in decibels above the top. We use
       * the quiet end in place of mute and clamp anything above normal to
       * normal. Write the file again only when that changes its contents. */
      {
         settings_t *settings = config_get_ptr();
         bool *muted_flag = audio_get_bool_ptr(AUDIO_ACTION_MUTE_ENABLE);
         float db = settings ? settings->floats.audio_volume : AUDIO_VOLUME_DEFAULT_DB;
         bool muted = muted_flag && *muted_flag;
         float snapped = rib_volume_quantize_db(muted ? AUDIO_VOLUME_MIN_DB : db);
         rib_set_volume_db(menu, snapped, muted || snapped != db);
      }
      RARCH_LOG("[RmlUi] Loaded menu from %s.\n", asset_directory);
   }

   if (rib_splash_active)
   {
      const retro_time_t now = menu_driver_get_current_time();
      retro_time_t elapsed;
      float opacity = 1.0f;
      if (!rib_splash_started_at)
         rib_splash_started_at = now;
      elapsed = now - rib_splash_started_at;
      if (elapsed >= 800000)
      {
         rib_splash_active = false;
         rib_rmlui_set_splash(false, 0.0f);
         if (!rib_splash_keep_menu_open)
         {
            retroarch_menu_running_finished(false);
            return;
         }
      }
      else
      {
         if (elapsed < 120000)
            opacity = (float)elapsed / 120000.0f;
         else if (elapsed > 550000)
            opacity = 1.0f - (float)(elapsed - 550000) / 250000.0f;
         rib_rmlui_set_splash(true, opacity);
         rib_rmlui_render((int)video_info->width, (int)video_info->height);
         return;
      }
   }

   menu_input_get_pointer_state(&pointer);
   {
      bool pointer_pressed =
            (pointer.flags & MENU_INP_PTR_FLG_PRESSED) != 0;

      rib_rmlui_pointer_move(pointer.x, pointer.y);
      rib_rmlui_pointer_button(pointer_pressed);

      if (menu->capture_active && pointer_pressed && !menu->pointer_pressed &&
            (rib_rmlui_hovered_action() == RIB_RMLUI_ACTION_CONTROLS_CANCEL ||
             rib_rmlui_hovered_action() == RIB_RMLUI_ACTION_CONTROLS_BACK))
         menu->capture_ignore_pointer = true;
      if (menu->capture_ignore_pointer && !pointer_pressed)
         menu->capture_ignore_pointer = false;
      menu->pointer_pressed = pointer_pressed;
   }

   {
      const char *drag_id = NULL;
      float drag_fraction = 0.0f;
      if (rib_rmlui_slider_drag(&drag_id, &drag_fraction) && drag_id
            && string_is_equal(drag_id, RIB_VOLUME_SLIDER_ID))
         rib_set_volume_db(menu,
               rib_volume_db_from_fraction(drag_fraction), false);
      /* In the frame where a screen appears, the track may not be laid out yet,
       * and a fill set from that width stays too short after the track grows.
       * We paint again on the next frames, with the width the player sees. */
      else
         rib_paint_volume();
   }

   /* Before we empty the queue, so we handle a scripted click in this frame,
    * in the same loop as a click from the player. */
   rib_rmlui_run_script();

   for (;;)
   {
      int next_action = rib_rmlui_take_action();
      if (next_action == RIB_RMLUI_ACTION_NONE)
         break;
      rib_rmlui_perform_action(menu, next_action);
   }

   if (menu->capture_active)
   {
      char capture_status[96];
      float remaining = 0.0f;
      enum menu_rib_bind_result result = menu_input_rib_bind_poll(
            menu_driver_get_current_time(), &remaining,
            !menu->capture_ignore_pointer);
      if (result == MENU_RIB_BIND_CAPTURED)
      {
         int conflict = rib_rmlui_find_binding_conflict(
               menu, menu->capture_control);
         menu->capture_active = false;
         rib_rmlui_restore_keyboard_mapping_bits();
         if (conflict >= 0)
         {
            snprintf(capture_status, sizeof(capture_status),
                  "SAVED; ALSO USED BY %s",
                  rib_control_console_name(menu, conflict));
            if (!rib_rmlui_save_controls(menu))
               strlcpy(capture_status, "BINDING ACTIVE; SAVE FAILED",
                     sizeof(capture_status));
            rib_rmlui_set_controls_status(capture_status);
         }
         else if (rib_rmlui_save_controls(menu))
            rib_rmlui_set_controls_status("BINDING SAVED");
         else
            rib_rmlui_set_controls_status("BINDING ACTIVE; SAVE FAILED");
         rib_rmlui_refresh_controls(menu);
         rib_rmlui_set_footer_hint("ESC  BACK");
      }
      else if (result == MENU_RIB_BIND_TIMED_OUT)
      {
         menu->capture_active = false;
         rib_rmlui_set_controls_status("TIMED OUT; BINDING UNCHANGED");
         rib_rmlui_set_footer_hint("ESC  BACK");
         rib_rmlui_refresh_controls(menu);
      }
      else
      {
         snprintf(capture_status, sizeof(capture_status),
               "%s: PRESS AN INPUT (%u)",
               rib_control_console_name(menu, menu->capture_control),
               (unsigned)(remaining + 0.999f));
         rib_rmlui_set_controls_status(capture_status);
      }
   }

   rib_rmlui_reload_if_changed();
   rib_rmlui_refresh_slots();
   rib_rmlui_render((int)video_info->width, (int)video_info->height);

}

/* A screen other than Pause and Controls, whose navigation we leave as it
 * was. On any other screen, the player moves through the parts of the panel in
 * the design, such as a slider, a toggle or a button, in document order. */
static int rib_part_navigate(rib_rmlui_menu_t *menu, enum menu_action action)
{
   char ids[16][64];
   const char *panel = rib_rmlui_screen_panel(menu->screen);
   int count = rib_rmlui_focusables(panel, ids, 16);

   if (count <= 0)
      return 0;
   if (menu->panel_focus < 0 || menu->panel_focus >= count)
      menu->panel_focus = 0;

   switch (action)
   {
      case MENU_ACTION_UP:
         menu->panel_focus = (menu->panel_focus + count - 1) % count;
         rib_rmlui_mark_focused(panel, ids[menu->panel_focus]);
#ifdef HAVE_AUDIOMIXER
         audio_driver_mixer_play_scroll_sound(true);
#endif
         return 0;
      case MENU_ACTION_DOWN:
         menu->panel_focus = (menu->panel_focus + 1) % count;
         rib_rmlui_mark_focused(panel, ids[menu->panel_focus]);
#ifdef HAVE_AUDIOMIXER
         audio_driver_mixer_play_scroll_sound(false);
#endif
         return 0;
      case MENU_ACTION_LEFT:
      case MENU_ACTION_RIGHT:
         if (rib_rmlui_part_is_slider(ids[menu->panel_focus]))
            rib_rmlui_nudge_slider(ids[menu->panel_focus],
                  action == MENU_ACTION_RIGHT ? 1 : -1);
         return 0;
      case MENU_ACTION_OK:
      case MENU_ACTION_SELECT:
         rib_rmlui_click_element(ids[menu->panel_focus]);
         return 0;
      case MENU_ACTION_CANCEL:
      case MENU_ACTION_RESUME:
      case MENU_ACTION_TOGGLE:
         rib_rmlui_play_action_sound(RIB_RMLUI_ACTION_CONTROLS_BACK);
         strlcpy(menu->screen, "pause", sizeof(menu->screen));
         rib_rmlui_show_screen("pause");
         rib_rmlui_mark_focused(panel, NULL);
         return 0;
      default:
         return 0;
   }
}

static int rib_rmlui_entry_action(void *data, menu_entry_t *entry,
      size_t index, enum menu_action action)
{
   rib_rmlui_menu_t *menu = (rib_rmlui_menu_t*)data;
   (void)entry;
   (void)index;

   if (!menu)
      return 0;

   if (menu->controls_visible)
   {
      if (menu->capture_active)
      {
         if (action == MENU_ACTION_CANCEL || action == MENU_ACTION_RESUME ||
             action == MENU_ACTION_TOGGLE)
            rib_rmlui_perform_action(menu, RIB_RMLUI_ACTION_CONTROLS_CANCEL);
         return 0;
      }

      switch (action)
      {
         case MENU_ACTION_UP:
         case MENU_ACTION_LEFT:
            menu->control_focus = rib_control_step(menu,
                  menu->control_focus, -1);
#ifdef HAVE_AUDIOMIXER
            audio_driver_mixer_play_scroll_sound(true);
#endif
            if (menu->control_focus < RIB_CONTROL_MAX)
               menu->selected_control = menu->control_focus;
            rib_rmlui_refresh_controls(menu);
            return 0;
         case MENU_ACTION_DOWN:
         case MENU_ACTION_RIGHT:
            menu->control_focus = rib_control_step(menu,
                  menu->control_focus, 1);
#ifdef HAVE_AUDIOMIXER
            audio_driver_mixer_play_scroll_sound(false);
#endif
            if (menu->control_focus < RIB_CONTROL_MAX)
               menu->selected_control = menu->control_focus;
            rib_rmlui_refresh_controls(menu);
            return 0;
         case MENU_ACTION_OK:
         case MENU_ACTION_SELECT:
            if (menu->control_focus < RIB_CONTROL_MAX)
               rib_rmlui_start_capture(menu, menu->control_focus);
            else if (menu->control_focus == RIB_CONTROL_MAX)
               rib_rmlui_perform_action(menu, RIB_RMLUI_ACTION_CONTROLS_RESET);
            else
               rib_rmlui_perform_action(menu, RIB_RMLUI_ACTION_CONTROLS_BACK);
            return 0;
         case MENU_ACTION_START:
            rib_rmlui_perform_action(menu, RIB_RMLUI_ACTION_CONTROLS_RESET);
            return 0;
         case MENU_ACTION_CANCEL:
         case MENU_ACTION_RESUME:
         case MENU_ACTION_TOGGLE:
            rib_rmlui_perform_action(menu, rib_rmlui_map_menu_toggle(
                  true, false));
         return 0;
      default:
         return 0;
      }
   }

   if (menu->screen[0] && !string_is_equal(menu->screen, "pause"))
      return rib_part_navigate(menu, action);

   switch (action)
   {
      case MENU_ACTION_UP:
         if (rib_rmlui_focus_is_slot(menu->focused))
         {
            int slot = rib_rmlui_focus_slot(menu->focused);
            if (slot > 3)
               rib_rmlui_focus(menu, menu->focused - 3, true);
            else
               rib_rmlui_focus(menu, RIB_RMLUI_ACTION_RESUME + slot - 1, true);
         }
         else
         {
            int action_index = menu->focused - RIB_RMLUI_ACTION_RESUME;
            int slot = 4 + (action_index > 2 ? 2 : action_index);
            rib_rmlui_focus(menu,
                  RIB_RMLUI_ACTION_SELECT_SLOT_1 + slot - 1, true);
         }
         return 0;
      case MENU_ACTION_DOWN:
         if (rib_rmlui_focus_is_slot(menu->focused))
         {
            int slot = rib_rmlui_focus_slot(menu->focused);
            if (slot <= 3)
               rib_rmlui_focus(menu, menu->focused + 3, false);
            else
               rib_rmlui_focus(menu, RIB_RMLUI_ACTION_RESUME + slot - 4, false);
         }
         else
         {
            int action_index = menu->focused - RIB_RMLUI_ACTION_RESUME;
            int slot = 1 + (action_index > 2 ? 2 : action_index);
            rib_rmlui_focus(menu,
                  RIB_RMLUI_ACTION_SELECT_SLOT_1 + slot - 1, false);
         }
         return 0;
      case MENU_ACTION_LEFT:
         if (rib_rmlui_focus_is_slot(menu->focused))
         {
            int slot = rib_rmlui_focus_slot(menu->focused);
            int row_start = slot <= 3 ? 1 : 4;
            slot = slot == row_start ? row_start + 2 : slot - 1;
            rib_rmlui_focus(menu,
                  RIB_RMLUI_ACTION_SELECT_SLOT_1 + slot - 1, true);
         }
         else
            rib_rmlui_focus(menu, menu->focused == RIB_RMLUI_ACTION_RESUME
                  ? RIB_RMLUI_ACTION_QUIT : menu->focused - 1, true);
         return 0;
      case MENU_ACTION_RIGHT:
         if (rib_rmlui_focus_is_slot(menu->focused))
         {
            int slot = rib_rmlui_focus_slot(menu->focused);
            int row_end = slot <= 3 ? 3 : 6;
            slot = slot == row_end ? row_end - 2 : slot + 1;
            rib_rmlui_focus(menu,
                  RIB_RMLUI_ACTION_SELECT_SLOT_1 + slot - 1, false);
         }
         else
            rib_rmlui_focus(menu, menu->focused == RIB_RMLUI_ACTION_QUIT
                  ? RIB_RMLUI_ACTION_RESUME : menu->focused + 1, false);
         return 0;
      case MENU_ACTION_OK:
      case MENU_ACTION_SELECT:
         rib_rmlui_perform_action(menu, menu->focused);
         return 0;
      case MENU_ACTION_CANCEL:
      case MENU_ACTION_RESUME:
      case MENU_ACTION_TOGGLE:
         rib_rmlui_perform_action(menu, rib_rmlui_map_menu_toggle(
               false, false));
         return 0;
      case MENU_ACTION_START:
         rib_rmlui_perform_action(menu, RIB_RMLUI_ACTION_SAVE);
         return 0;
      default:
         return 0;
   }
}

static int rib_rmlui_bind_init(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t index)
{
   (void)cbs;
   (void)path;
   (void)label;
   (void)type;
   (void)index;
   return 0;
}

menu_ctx_driver_t menu_ctx_rmlui = {
   .frame          = rib_rmlui_frame,
   .init           = rib_rmlui_menu_init,
   .free           = rib_rmlui_free,
   .context_reset  = rib_rmlui_context_reset,
   .context_destroy = rib_rmlui_context_destroy,
   .bind_init      = rib_rmlui_bind_init,
   .ident          = "rmlui",
   .toggle         = rib_rmlui_toggle,
   .entry_action   = rib_rmlui_entry_action
};
