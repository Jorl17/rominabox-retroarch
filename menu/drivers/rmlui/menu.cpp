#include <stdlib.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include <new>
#include <stdint.h>
#include <retro_miscellaneous.h>
#include <file/file_path.h>
#include <file/config_file.h>
#include <streams/file_stream.h>
#include <string/stdstring.h>
#include "../../../verbosity.h"
#include "../rmlui_bridge.h"
#include "../rmlui_shader_mark.h"
#include "menu_api.h"
#include "files.h"
#include "host.h"
#include "declarations.h"
#include "overlays.hpp"
#include "script.hpp"

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
/* How many generated rows fit in one list, the size of a buffer. We write the
 * ids at export, and we log any id beyond this number instead of applying it
 * to the wrong preset. */
#define RIB_SHADER_MAX 32

typedef struct rib_control
{
   char id[32];
   /* Empty when we draw the control by itself. The directions of a stick have
    * one name, and the list for any of them contains every direction. */
   char group[32];
   unsigned bind_index;
} rib_control_t;

typedef struct rib_rmlui_menu
{
   bool initialized;
   bool overlay_mode;
   rib::Overlays overlays;
   rib::Script script;
   bool pointer_pressed;
   bool transfer_pending;
   bool transfer_is_save;
   int selected_slot;
   int transfer_slot;
   int focused;
   bool controls_visible;
   bool controls_loaded;
   int panel_focus;
   /* Which button of the pause row is focused, as an index into that row,
    * or -1 while a save slot is focused instead. */
   int row_focus;
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
   /* The screen shown now. "pause" is the main menu screen. Escape resumes
    * the game only on pause, and on any other screen it stays in the menu. */
   char screen[32];
   int list_focus;
   char shader_ids[RIB_SHADER_MAX][64];
   char shader_presets[RIB_SHADER_MAX][PATH_MAX_LENGTH];
   int shader_count;
   char shader_state_on[32];
   char shader_state_off[32];
   rib_toggle_t toggles[RIB_TOGGLE_MAX];
   int toggle_count;
} rib_rmlui_menu_t;

/* The public runloop callback can come before we allocate the menu. Keep only
 * that pending request here, and the overlay timeline in the menu itself. */
static bool pending_overlay_start;
static rib_rmlui_menu_t *rib_rmlui_active_menu;

/* The bind list. The element, and how long a control stays current before we
 * open the list, are in the design. Here we only keep the time. */
static char rib_binds_list[64];
static int rib_binds_after_ms;
static int rib_binds_width;
static int rib_binds_for = -1;
static int64_t rib_binds_since;
static bool rib_binds_open;

static void rib_rmlui_refresh_controls(rib_rmlui_menu_t *menu);
static void rib_focus_control(rib_rmlui_menu_t *menu, int index);
static void rib_focus_list(rib_rmlui_menu_t *menu, int index);
static void rib_callout_text(const rib_rmlui_menu_t *menu, int index,
      char *out, size_t length);
static void rib_rmlui_cancel_capture(rib_rmlui_menu_t *menu, const char *status);
static void rib_rmlui_load_shaders(rib_rmlui_menu_t *menu,
      const char *asset_directory);
static bool rib_rmlui_apply_listed_shader(rib_rmlui_menu_t *menu, const char *id);
int rib_menu_key(void *data, enum rib_key action);

void rib_rmlui_begin_overlays(void)
{
   if (rib_rmlui_active_menu)
      rib_rmlui_active_menu->overlays.begin();
   else
   {
      pending_overlay_start = true;
      rib_host_overlay_frames(true);
   }
}

bool rib_rmlui_overlays_drawing(void)
{
   return pending_overlay_start ||
         (rib_rmlui_active_menu && rib_rmlui_active_menu->overlays.drawing());
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
      if (!rib_host_bind_index(id, &bind_index))
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
      menu->controls[menu->control_count].group[0] = '\0';
      {
         char group_key[96];
         snprintf(group_key, sizeof(group_key), "rib_group_%s", id);
         config_get_array(config, group_key,
               menu->controls[menu->control_count].group,
               sizeof(menu->controls[menu->control_count].group));
      }
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
/* The list whose rows are disc images, and the screen with the button for
 * that list when the core has more than one image. We read both from
 * design.cfg (screen_images_ / screen_mark_) and copy nothing from a design. */
static char rib_disc_list_id[32];
static char rib_disc_list_button[32];
static char rib_disc_mark[32];
static char rib_disc_redirect_from[32];
static char rib_disc_redirect_to[32];

/* We export the entry hidden, because get_num_images is unknown until the
 * game has loaded, and we cannot create the entry in the player then. It stays
 * hidden for a game with one disc, so the focus never stops on an empty spot. */
static void rib_discs_sync(void)
{
   char list_id[48];
   unsigned count;
   unsigned current;
   int rows;
   int index;

   if (!rib_disc_list_id[0])
      return;
   count = rib_host_disc_count();
   if (rib_disc_list_button[0])
   {
      rib_rmlui_set_shown(rib_disc_list_button, count > 1);
      rib_rmlui_set_disabled(rib_disc_list_button, count <= 1);
   }
   snprintf(list_id, sizeof(list_id), "%s-list", rib_disc_list_id);
   rows = rib_rmlui_rows_in(list_id);
   current = rib_host_disc_index();
   for (index = 0; index < rows; index++)
   {
      const char *row = rib_rmlui_row_in(list_id, index);
      char label[PATH_MAX_LENGTH];

      if (!row)
         continue;
      if ((unsigned)index >= count)
      {
         rib_rmlui_set_shown(row, false);
         continue;
      }
      label[0] = '\0';
      rib_host_disc_label((unsigned)index, label, sizeof(label));
      if (!label[0])
         snprintf(label, sizeof(label), "Disc %u", (unsigned)index + 1);
      rib_rmlui_set_shown(row, true);
      rib_rmlui_fit_row_title(row, label);
   }
   if (rows > 0)
      rib_rmlui_retarget_pages(list_id);
   if (count > 0 && current < (unsigned)rows)
   {
      const char *row = rib_rmlui_row_in(list_id, (int)current);

      if (row)
         rib_rmlui_select_row(list_id, row, rib_disc_mark, "");
   }
   {
      char status_id[40];
      char status[64];

      snprintf(status_id, sizeof(status_id), "%s-status", rib_disc_list_id);
      status[0] = '\0';
      if (rows > 0 && count > (unsigned)rows)
         snprintf(status, sizeof(status), "SHOWING %d OF %u", rows, count);
      rib_rmlui_set_element_text(status_id, status);
   }
}

/* The image index is the document order. We do not parse the row id, because
 * with a hidden row the id and the document order would differ. */
static bool rib_discs_choose(rib_rmlui_menu_t *menu, const char *id)
{
   char list_id[48];
   unsigned count;
   int rows;
   int index;

   if (!menu || !id || !rib_disc_list_id[0])
      return false;
   if (!string_is_equal(menu->screen, rib_disc_list_id))
      return false;
   snprintf(list_id, sizeof(list_id), "%s-list", rib_disc_list_id);
   rows = rib_rmlui_rows_in(list_id);
   count = rib_host_disc_count();
   for (index = 0; index < rows; index++)
   {
      const char *row = rib_rmlui_row_in(list_id, index);
      unsigned image;

      if (!row || !string_is_equal(row, id))
         continue;
      if ((unsigned)index >= count)
         return true;
      image = (unsigned)index;
      rib_host_choose_disc(image);
      return true;
   }
   return false;
}

static const char *rib_absolute_data_dir(void)
{
   const char *data = getenv("ROMINABOX_DATA_DIR");

   if (!data || data[0] != '/')
      return NULL;
   return data;
}

/* Where we store the position of a switch, in the game's storage. */
static bool rib_toggle_path(const char *id, char *out, size_t length)
{
   const char *data = rib_absolute_data_dir();

   if (!data || !*data || !id || !*id)
      return false;
   snprintf(out, length, "%s/toggle-%s", data, id);
   return true;
}

static void rib_toggle_remember(const rib_toggle_t *toggle)
{
   char path[PATH_MAX_LENGTH];
   const char *body = toggle->state ? "1\n" : "0\n";

   if (!rib_toggle_path(toggle->id, path, sizeof(path)))
      return;
   if (!filestream_write_file(path, body, (int64_t)strlen(body)))
      RARCH_ERR("[RIB] the switch '%s' is %s, but %s could not be written, so "
            "the next launch will start from the design's default.\n",
            toggle->id, toggle->state ? "on" : "off", path);
}

static bool rib_toggle_recall(rib_toggle_t *toggle)
{
   char path[PATH_MAX_LENGTH];
   int64_t length = 0;
   char *body = NULL;

   if (!rib_toggle_path(toggle->id, path, sizeof(path)))
      return false;
   if (!filestream_read_file(path, (void**)&body, &length) || !body)
      return false;
   toggle->state = length > 0 && body[0] == '1';
   free(body);
   return true;
}

/* We load the declarations once. These assignments set the starting state of
 * each feature. There is no document or input code in the loader. */
static void rib_rmlui_load_design(rib_rmlui_menu_t *menu, const char *assets)
{
   rib_design_declarations *loaded = rib_load_design(assets);
   const rib_design_data *design = rib_design_get(loaded);
   size_t index;
   rib_rmlui_clear_screens();
   rib_disc_list_id[0] = '\0';
   rib_disc_list_button[0] = '\0';
   rib_disc_mark[0] = '\0';
   rib_disc_redirect_from[0] = '\0';
   rib_disc_redirect_to[0] = '\0';
   for (index = 0; index < design->screen_count; ++index)
   {
      const rib_screen_declaration *screen = &design->screens[index];
      if (string_is_equal(screen->images, "list"))
      {
         strlcpy(rib_disc_list_id, screen->id, sizeof(rib_disc_list_id));
         strlcpy(rib_disc_list_button, screen->button, sizeof(rib_disc_list_button));
         strlcpy(rib_disc_mark, screen->mark, sizeof(rib_disc_mark));
      }
      else if (screen->images[0])
      {
         strlcpy(rib_disc_redirect_from, screen->id, sizeof(rib_disc_redirect_from));
         strlcpy(rib_disc_redirect_to, screen->images, sizeof(rib_disc_redirect_to));
      }
      rib_rmlui_declare_screen(screen->id, screen->panel, screen->heading,
            screen->footer, screen->button);
   }
   menu->toggle_count = design->toggle_count;
   for (index = 0; index < (size_t)menu->toggle_count; ++index)
   {
      menu->toggles[index] = design->toggles[index];
      rib_toggle_recall(&menu->toggles[index]);
   }
   menu->overlays.load(*design);
   strlcpy(rib_binds_list, design->binds_list, sizeof(rib_binds_list));
   rib_binds_after_ms = design->binds_after_ms;
   rib_binds_width = design->binds_width;
   rib_design_free(loaded);
}

/* The combined effect of all switches. We combine them instead of applying
 * them in turn, so of two switches that lock the slots, the last does not win. */
static void rib_rmlui_apply_toggles(rib_rmlui_menu_t *menu)
{
   const rib_toggle_t *guarding = NULL;
   int index;

   if (!menu)
      return;
   for (index = 0; index < menu->toggle_count; ++index)
   {
      const rib_toggle_t *toggle = &menu->toggles[index];
      rib_rmlui_set_toggle(toggle->id,
            toggle->state ? toggle->on : toggle->off, toggle->state);
      if (toggle->state && toggle->guard == RIB_TOGGLE_GUARD_SAVES && !guarding)
         guarding = toggle;
   }
   rib_rmlui_guard_slots(guarding ? guarding->guard_label : NULL,
         guarding ? guarding->guard_status : NULL);
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

const char *rib_rmlui_control_group(int index)
{
   const rib_rmlui_menu_t *menu = rib_rmlui_active_menu;
   if (!menu || index < 0 || index >= menu->control_count)
      return NULL;
   if (!menu->controls[index].group[0])
      return NULL;
   return menu->controls[index].group;
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

/* The buttons of the pause row, in document order, without the disabled
 * ones.
 *
 * The pause row contains what the design and the export put in it. With
 * Options on, Options is the fourth button, and a game without Options has
 * one button fewer.
 *
 * We leave disabled buttons out of the list and do not skip them when the
 * focus reaches them, because LOAD is disabled until there is a save.
 */
static int rib_pause_row(char ids[][64], int capacity)
{
   char all[16][64];
   int found = rib_rmlui_focusables("pause-panel", all, 16);
   int count = 0;
   int index;

   for (index = 0; index < found && count < capacity; ++index)
   {
      if (rib_rmlui_element_disabled(all[index]))
         continue;
      strlcpy(ids[count], all[index], 64);
      ++count;
   }
   return count;
}

/* The position of the focused button in that row now. The row can change while
 * a button is focused, for example LOAD becomes usable once a slot has a save,
 * so we look it up on every move and do not store it. */
static int rib_pause_row_index(const char ids[][64], int count)
{
   const char *focused = rib_rmlui_focused_element();
   int index;

   if (focused && *focused)
      for (index = 0; index < count; ++index)
         if (string_is_equal(ids[index], focused))
            return index;
   return 0;
}

static void rib_pause_focus_row(rib_rmlui_menu_t *menu, int index,
      bool direction_up)
{
   char ids[16][64];
   const int count = rib_pause_row(ids, 16);

   if (count <= 0)
      return;
   index = ((index % count) + count) % count;
   menu->row_focus = index;
   rib_rmlui_focus_element(ids[index]);
#ifdef HAVE_AUDIOMIXER
   rib_host_scroll_sound(direction_up);
#endif
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
   /* Focusing an action or a slot here moves the focus off the row. We track
    * the row by id, not by this enum. */
   menu->row_focus = -1;
   rib_rmlui_set_focused(focused);
   if (rib_rmlui_focus_is_slot(focused))
      rib_rmlui_select_slot(menu, rib_rmlui_focus_slot(focused));
#ifdef HAVE_AUDIOMIXER
   if (changed)
      rib_host_scroll_sound(direction_up);
#endif
}

static bool rib_rmlui_load_is_available(const rib_rmlui_menu_t *menu)
{
   return menu && rib_host_slot_occupied(menu->selected_slot);
}

static void rib_rmlui_refresh_slots(void)
{
   int slot;
   for (slot = 1; slot <= 6; ++slot)
   {
      char thumbnail_path[PATH_MAX_LENGTH] = {0};
      bool occupied = rib_host_slot_occupied(slot);
      rib_host_thumbnail(slot, thumbnail_path, sizeof(thumbnail_path));
      rib_rmlui_set_game_aspect(rib_host_game_aspect());
      rib_rmlui_set_slot_state(slot, occupied, thumbnail_path);
   }
}

static bool rib_rmlui_begin_transfer(rib_rmlui_menu_t *menu, bool is_save)
{
   if (!menu || menu->transfer_pending)
      return false;

   menu->transfer_path[0] = '\0';
   if (!rib_host_state_path(menu->selected_slot, menu->transfer_path,
         sizeof(menu->transfer_path)))
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
         menu->control_labels[index][0] = '\0';
         rib_host_clear_bind(menu->controls[index].bind_index);
      }
      snprintf(key, sizeof(key), "rib_label_%s", menu->controls[index].id);
      if (config_get_array(config, key, label, sizeof(label)))
         strlcpy(menu->control_labels[index], label,
               sizeof(menu->control_labels[index]));
      rib_host_load_bind(config, menu->controls[index].id, menu->controls[index].bind_index);

   }
   rib_host_restore_keyboard_mapping();
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
   rib_focus_control(menu, rib_control_first(menu));
   rib_rmlui_refresh_controls(menu);
}

static bool rib_rmlui_save_controls(rib_rmlui_menu_t *menu)
{
   config_file_t *config;
   bool saved;
   int index;

   if (!menu || !menu->controls_path[0] || !(config = config_file_new_alloc()))
      return false;

   config_set_string(config, "controls_profile", menu->profile_id);
   for (index = 0; index < menu->control_count; ++index)
   {
      char key[96];

      if (!rib_control_is_active(menu, index))
         continue;
      snprintf(key, sizeof(key), "rib_label_%s", menu->controls[index].id);
      config_set_string(config, key, menu->control_labels[index]);

      rib_host_write_bind(config, menu->controls[index].id, menu->controls[index].bind_index);
   }

   saved = rib_write_menu_config(config, menu->controls_path,
         RIB_CONFIG_WRITE_CONTROLS);
   config_file_free(config);
   return saved;
}

static void rib_rmlui_refresh_controls(rib_rmlui_menu_t *menu)
{
   int index;
   for (index = 0; menu && index < menu->control_count; ++index)
   {
      char display_label[NAME_MAX_LENGTH * 2];
      char binding[4096];
      char group_id[96];

      if (!rib_control_is_active(menu, index))
         continue;
      strlcpy(display_label, menu->control_labels[index],
            sizeof(display_label));
      rib_callout_text(menu, index, binding, sizeof(binding));
      rib_rmlui_set_control_state(menu->controls[index].id,
            display_label, binding,
            menu->controls_visible && menu->control_focus == index,
            menu->capture_active && menu->capture_control == index);
      if (menu->controls[index].group[0])
      {
         snprintf(group_id, sizeof(group_id), "control-group-binding-%s",
               menu->controls[index].group);
         rib_rmlui_set_element_text(group_id, binding);
      }
   }
   rib_rmlui_set_controls_action_focus(
         menu && menu->controls_visible && menu->control_focus == RIB_CONTROL_MAX,
         menu && menu->controls_visible && menu->control_focus == RIB_CONTROL_MAX + 1,
         menu && menu->capture_active);
   if (menu && menu->controls_visible && menu->control_focus >= 0
         && menu->control_focus < menu->control_count
         && menu->controls[menu->control_focus].group[0])
      rib_rmlui_focus_group(menu->controls[menu->control_focus].group);
   else
      rib_rmlui_focus_group(NULL);
}

static void rib_rmlui_reset_interaction(rib_rmlui_menu_t *menu, bool opening)
{
   if (!menu)
      return;
   if (menu->capture_active)
      rib_rmlui_cancel_capture(menu, NULL);
   menu->controls_visible = false;
   strlcpy(menu->screen, "pause", sizeof(menu->screen));
   rib_focus_list(menu, 0);
   menu->pointer_pressed = false;
   menu->capture_ignore_pointer = false;
   menu->focused = RIB_RMLUI_ACTION_RESUME;
   /* When the menu opens, we focus the first button of the row, not a slot. */
   menu->row_focus = 0;
   rib_rmlui_clear_intents();
   rib_rmlui_pointer_leave();
   if (opening)
   {
      rib_rmlui_show_controls(false);
      rib_rmlui_set_focused(menu->focused);
      rib_rmlui_set_footer_hint("ESC  CONTINUE");
   }
}

void rib_menu_toggle(void *userdata, bool on)
{
   rib_rmlui_reset_interaction((rib_rmlui_menu_t*)userdata, on);
}

bool rib_menu_consume_toggle(void *userdata)
{
   rib_rmlui_menu_t *menu = (rib_rmlui_menu_t*)userdata;
   return menu && rib_rmlui_toggle_stays_in_menu(
         menu->controls_visible || !string_is_equal(menu->screen, "pause"),
         menu->capture_active);
}

static void rib_rmlui_cancel_capture(rib_rmlui_menu_t *menu,
      const char *status)
{
   if (!menu || !menu->capture_active)
      return;
   rib_host_capture_cancel();
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
   if (!rib_host_capture_start(menu->controls[index].bind_index,
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
   int index;
   if (!rib_control_is_active(menu, changed_index))
      return -1;
   for (index = 0; index < menu->control_count; ++index)
   {
      if (index == changed_index || !rib_control_is_active(menu, index))
         continue;
      if (rib_host_bind_conflicts(menu->controls[changed_index].bind_index,
               menu->controls[index].bind_index))
         return index;
   }
   return -1;
}

/* A step that changed the level. We play the up or down wav from the sound
 * pack, the same cue as for a focus move, and not the confirm cue. */
void rib_rmlui_play_move_sound(int direction)
{
#ifdef HAVE_AUDIOMIXER
   if (direction != 0)
      rib_host_scroll_sound(direction > 0);
#else
   (void)direction;
#endif
}

static void rib_rmlui_play_action_sound(int action)
{
#ifdef HAVE_AUDIOMIXER
   switch (rib_rmlui_action_sound(action))
   {
      case RIB_MENU_SOUND_OK:
         rib_host_ok_sound();
         break;
      case RIB_MENU_SOUND_CANCEL:
         rib_host_cancel_sound();
         break;
      case RIB_MENU_SOUND_NONE:
         break;
   }
#endif
}

static bool rib_save_volume(const rib_rmlui_menu_t *menu, float db)
{
   if (!menu || !menu->volume_path[0])
      return false;
   return rib_write_menu_volume(menu->volume_path, db);
}

static void rib_paint_volume(void)
{
   float db = rib_host_volume();

   db = rib_volume_quantize_db(db);
   /* No readout. Low and high are in the design, and the position of the
    * thumb is the value. With an empty string we clear what we wrote before. */
   rib_rmlui_set_slider(RIB_VOLUME_SLIDER_ID,
         rib_volume_fraction_from_db(db), "");
}

static void rib_set_volume_db(rib_rmlui_menu_t *menu, float db, bool persist)
{
   db = rib_volume_quantize_db(db);
   rib_host_set_volume(db);
   if (persist)
      rib_save_volume(menu, db);
   rib_paint_volume();
}

/* The bundled list, from shaders.cfg next to the design. An id that is not in
 * it is a row of another list, and choosing it has no effect here. */
static void rib_rmlui_load_shaders(rib_rmlui_menu_t *menu,
      const char *asset_directory)
{
   char path[PATH_MAX_LENGTH];
   config_file_t *config;
   char list[1024];
   char *cursor;
   char *token;

   if (!menu)
      return;
   menu->shader_count = 0;
   menu->shader_state_on[0] = '\0';
   menu->shader_state_off[0] = '\0';
   if (!asset_directory || !*asset_directory)
      return;
   snprintf(path, sizeof(path), "%s/shaders.cfg", asset_directory);
   if (!(config = config_file_new_from_path_to_string(path)))
      return;
   config_get_array(config, "shader_state_on",
         menu->shader_state_on, sizeof(menu->shader_state_on));
   config_get_array(config, "shader_state_off",
         menu->shader_state_off, sizeof(menu->shader_state_off));
   if (!config_get_array(config, "shader_ids", list, sizeof(list)))
   {
      config_file_free(config);
      return;
   }
   cursor = list;
   while ((token = strtok_r(cursor, " ", &cursor)))
   {
      char key[96];
      char preset[PATH_MAX_LENGTH];

      if (!*token)
         continue;
      if (menu->shader_count >= RIB_SHADER_MAX)
      {
         RARCH_ERR("[RIB] shader list has more than %d entries; the rest "
               "are not offered.\n", RIB_SHADER_MAX);
         break;
      }
      strlcpy(menu->shader_ids[menu->shader_count], token,
            sizeof(menu->shader_ids[menu->shader_count]));
      snprintf(key, sizeof(key), "shader_preset_%s", token);
      preset[0] = '\0';
      config_get_array(config, key, preset, sizeof(preset));
      strlcpy(menu->shader_presets[menu->shader_count], preset,
            sizeof(menu->shader_presets[menu->shader_count]));
      menu->shader_count++;
   }
   config_file_free(config);
}

/* In the staged document the author's starting preset is still marked on.
 * The preset that is on is the one running in RetroArch, which we load at
 * launch after a restart, or apply when the player clicks a row. Mark that
 * row only when this list is on screen, because every list uses this action. */
static void rib_rmlui_show_running_shader(rib_rmlui_menu_t *menu)
{
   const char *relatives[RIB_SHADER_MAX];
   const char *current;
   int index;
   int row;
   int rows;
   int matched = -1;
   bool ours   = false;

   if (!menu || menu->shader_count <= 0)
      return;
   rows = rib_rmlui_visible_row_count();
   for (row = 0; row < rows && !ours; ++row)
   {
      const char *id = rib_rmlui_list_row_id(row);

      for (index = 0; index < menu->shader_count; ++index)
         if (id && string_is_equal(id, menu->shader_ids[index]))
            ours = true;
   }
   if (!ours)
      return;

   for (index = 0; index < menu->shader_count; ++index)
      relatives[index] = menu->shader_presets[index];
   current = rib_host_current_shader();
   matched = rib_shader_mark_index(current, relatives, menu->shader_count);
   if (matched < 0)
   {
      fprintf(stderr, "[RIB] no bundled shader matches the one running: %s\n",
            current && current[0] ? current : "none");
      rib_rmlui_mark_row("", menu->shader_state_on, menu->shader_state_off);
      return;
   }
   fprintf(stderr, "[RIB] shader row '%s' is the one running\n",
         menu->shader_ids[matched]);
   rib_rmlui_mark_row(menu->shader_ids[matched],
         menu->shader_state_on, menu->shader_state_off);
}

static bool rib_rmlui_apply_listed_shader(rib_rmlui_menu_t *menu, const char *id)
{
   const char *assets = getenv("ROMINABOX_RML_ASSETS");
   const char *data = rib_absolute_data_dir();
   char absolute[PATH_MAX_LENGTH];
   char choice_path[PATH_MAX_LENGTH];
   char body[PATH_MAX_LENGTH + 2];
   const char *relative = NULL;
   int index;
   bool known = false;

   if (!menu || !id || !*id || !rib_host_has_settings())
      return false;
   for (index = 0; index < menu->shader_count; ++index)
      if (string_is_equal(menu->shader_ids[index], id))
      {
         relative = menu->shader_presets[index];
         known = true;
         break;
      }
   /* Not in this list. Other lists use the same action, and we handle their
    * ids with each list. */
   if (!known)
      return false;

   absolute[0] = '\0';
   if (relative && *relative && assets && *assets)
      snprintf(absolute, sizeof(absolute), "%s/%s", assets, relative);

   rib_host_apply_shader(id, absolute);

   if (data && *data)
   {
      snprintf(choice_path, sizeof(choice_path), "%s/shader-choice", data);
      if (absolute[0])
         snprintf(body, sizeof(body), "%s\n", absolute);
      else
         strlcpy(body, "\n", sizeof(body));
      if (!filestream_write_file(choice_path, body, (int64_t)strlen(body)))
         RARCH_ERR("[RIB] the shader is active, but %s could not be written. "
               "The next launch will use the bundled starting shader.\n",
               choice_path);
   }
   rib_rmlui_show_running_shader(menu);
   return true;
}

/* The position of the keyboard focus on a list screen. The rows come first, then
 * the other controls of the screen, so moving down past the last row reaches the
 * switch and BACK. A player with a pad could not use a switch that only a
 * pointer can reach. */
static void rib_rmlui_focus_list(rib_rmlui_menu_t *menu)
{
   const int rows = rib_rmlui_visible_row_count();

   if (!menu)
      return;
   if (menu->list_focus < rows)
   {
      rib_rmlui_focus_list_row(menu->list_focus);
      rib_rmlui_focus_list_control(-1);
      return;
   }
   rib_rmlui_focus_list_row(-1);
   rib_rmlui_focus_list_control(menu->list_focus - rows);
}

/* The only place where we write list_focus. We call it for the keys and for
 * the pointer, in every list, the shader list included. */
static void rib_focus_list(rib_rmlui_menu_t *menu, int index)
{
   if (!menu || index < 0)
      return;
   menu->list_focus = index;
   rib_rmlui_focus_list(menu);
}

static void rib_rmlui_perform_action(rib_rmlui_menu_t *menu, const rib::Event& event)
{
   const auto action = event.kind;
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
      if (string_is_equal(event.id.c_str(), RIB_VOLUME_SLIDER_ID))
         rib_set_volume_db(menu,
               rib_volume_db_from_fraction(event.fraction), true);
      return;
   }
   if (action == RIB_RMLUI_ACTION_LIST_CHOOSE)
   {
      const char *id = event.id.c_str();
      int row;

      if (rib_discs_choose(menu, id))
      {
         rib_rmlui_play_action_sound(action);
         rib_discs_sync();
      }
      else if (rib_rmlui_apply_listed_shader(menu, id))
         rib_rmlui_play_action_sound(action);
      for (row = 0; row < rib_rmlui_visible_row_count(); ++row)
         if (string_is_equal(rib_rmlui_list_row_id(row), id))
         {
            rib_focus_list(menu, row);
            break;
         }
      return;
   }
   if (action == RIB_RMLUI_ACTION_LIST_PAGE)
   {
      const char *which = event.id.c_str();
      int delta = which && string_is_equal(which, "prev") ? -1 : 1;

      rib_rmlui_play_action_sound(action);
      if (rib_rmlui_turn_list_page(delta) >= 0)
         rib_focus_list(menu, 0);
      return;
   }
   if (action == RIB_RMLUI_ACTION_PART_TOGGLE)
   {
      rib_rmlui_play_action_sound(action);
      return;
   }
   if (action == RIB_RMLUI_ACTION_TOGGLE)
   {
      const char *id = event.id.c_str();
      int index;

      rib_rmlui_play_action_sound(action);
      /* We save the state of a list switch, and only repaint the other parts. */
      if (!id || !*id)
         return;
      for (index = 0; index < menu->toggle_count; ++index)
      {
         rib_toggle_t *toggle = &menu->toggles[index];

         if (!string_is_equal(toggle->id, id))
            continue;
         toggle->state = !toggle->state;
         rib_toggle_remember(toggle);
         rib_rmlui_apply_toggles(menu);
         break;
      }
      return;
   }
   if (action == RIB_RMLUI_ACTION_SHOW_SCREEN)
   {
      /* We capture and navigate on the active screen, with the id from the event. */
      const char *wanted = event.id.c_str();
      char screen_id[32];

      screen_id[0] = '\0';
      if (wanted && *wanted)
         strlcpy(screen_id, wanted, sizeof(screen_id));
      /* Pressing the button in the column opens the circle. With more than one
       * image, we make it open the list instead, because a second button would
       * move the column, and hiding the only button would leave a gap. */
      if (screen_id[0]
            && rib_disc_redirect_from[0]
            && string_is_equal(screen_id, rib_disc_redirect_from)
            && rib_host_disc_count() > 1
            && rib_disc_redirect_to[0])
         strlcpy(screen_id, rib_disc_redirect_to, sizeof(screen_id));
      if (screen_id[0] && rib_rmlui_show_screen(screen_id))
      {
         /* The footer and the heading are in the design, with the screen.
          * Here we keep only the case of the controls screen, where capture
          * and navigation work differently. For any other screen there is
          * nothing to add here. */
         strlcpy(menu->screen, screen_id, sizeof(menu->screen));
         menu->controls_visible = string_is_equal(screen_id, "controls");
         rib_focus_list(menu, 0);
         if (menu->controls_visible)
         {
            rib_focus_control(menu, rib_control_first(menu));
            rib_rmlui_set_controls_status("SELECT A CONTROL TO REBIND");
            rib_rmlui_refresh_controls(menu);
         }
         else if (menu->capture_active)
            rib_rmlui_cancel_capture(menu, "BINDING UNCHANGED");
         else if (!string_is_equal(screen_id, "pause"))
         {
            char ids[16][64];
            const char *panel = rib_rmlui_screen_panel(menu->screen);
            int count = rib_rmlui_focusables(panel, ids, 16);
            bool slider = false;
            int index;

            for (index = 0; index < count; ++index)
               if (rib_rmlui_part_is_slider(ids[index]))
                  slider = true;
            /* A slider is the first thing a keyboard should land on: left and
             * right move it. A list with no slider focuses its first row, and
             * then the screen's own controls past that. */
            if (slider)
            {
               menu->panel_focus = 0;
               rib_rmlui_mark_focused(panel, ids[0]);
            }
            else
               rib_rmlui_focus_list(menu);
         }
         /* We measure the slider from the box of its track. While the panel
          * is hidden that width is zero, so a paint leaves the thumb where
          * the stylesheet put it, at the quiet end. Paint it again now that
          * the screen is shown. */
         rib_paint_volume();
         rib_rmlui_show_running_shader(menu);
      }
      rib_rmlui_play_action_sound(action);
      return;
   }

   if (action == RIB_RMLUI_ACTION_DEVICE_PICKER_CHOOSE)
   {
      /* The chosen id is in the event, which we keep until we apply this choice. */
      const char *chosen = event.id.c_str();
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
         if (known)
            rib_host_apply_device(chosen, device);

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
         rib_focus_control(menu, control_index);
         rib_rmlui_start_capture(menu, control_index);
      }
      return;
   }

   if ((action == RIB_RMLUI_ACTION_SAVE ||
            action == RIB_RMLUI_ACTION_LOAD) &&
         (menu->transfer_pending || rib_rmlui_slots_guarded()))
      return;
   if (rib_rmlui_slots_guarded() && rib_rmlui_focus_is_slot(action))
      return;

   if (action == RIB_RMLUI_ACTION_LOAD && !rib_rmlui_load_is_available(menu))
      return;
   rib_rmlui_play_action_sound(action);

   switch (action)
   {
      case RIB_RMLUI_ACTION_SAVE:
         if (!rib_rmlui_begin_transfer(menu, true))
            return;
         rib_host_select_state_slot(menu->selected_slot);
         snprintf(status, sizeof(status), "SAVING SLOT %d...",
               menu->selected_slot);
         rib_rmlui_set_status(status);
         if (!rib_host_save_state() &&
               menu->transfer_pending)
            rib_rmlui_notify_state_task(menu->transfer_path,
                  menu->transfer_slot, true, false);
         break;
      case RIB_RMLUI_ACTION_LOAD:
         if (!rib_rmlui_load_is_actionable(rib_rmlui_load_is_available(menu)))
            return;
         if (!rib_rmlui_begin_transfer(menu, false))
            return;
         rib_host_select_state_slot(menu->selected_slot);
         snprintf(status, sizeof(status), "LOADING SLOT %d...",
               menu->selected_slot);
         rib_rmlui_set_status(status);
         if (!rib_host_load_state() &&
               menu->transfer_pending)
            rib_rmlui_notify_state_task(menu->transfer_path,
                  menu->transfer_slot, false, false);
         break;
      case RIB_RMLUI_ACTION_CONTROLS:
      {
         /* Open the screen on the pause row in the design, through its button.
          * We use this path for both the keyboard and the pointer. */
         const char *button = rib_rmlui_pause_screen_button();

         if (button && *button)
         {
            rib_rmlui_click_element(button);
            return;
         }
      }
         strlcpy(menu->screen, "controls", sizeof(menu->screen));
         menu->controls_visible = true;
         strlcpy(menu->screen, "controls", sizeof(menu->screen));
         rib_focus_control(menu, rib_control_first(menu));
         /* The heading and the footer are in the design, with the
          * screen. */
         rib_rmlui_show_screen("controls");
         rib_rmlui_set_controls_status("SELECT A CONTROL TO REBIND");
         rib_rmlui_refresh_controls(menu);
         break;
      case RIB_RMLUI_ACTION_CONTROLS_BACK:
         if (menu->capture_active)
            rib_rmlui_cancel_capture(menu, "BINDING UNCHANGED");
         strlcpy(menu->screen, "pause", sizeof(menu->screen));
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
         rib_host_resume();
         break;
      case RIB_RMLUI_ACTION_QUIT:
         rib_host_quit();
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

void *rib_menu_create(void)
{
   rib_rmlui_menu_t *menu = new (std::nothrow) rib_rmlui_menu_t{};
   if (!menu)
      return nullptr;
   menu->selected_slot = 1;
   menu->focused = RIB_RMLUI_ACTION_RESUME;
   menu->row_focus = 0;
   strlcpy(menu->screen, "pause", sizeof(menu->screen));
   rib_rmlui_active_menu = menu;
   if (pending_overlay_start)
   {
      pending_overlay_start = false;
      menu->overlays.begin();
   }
   return menu;
}

void rib_menu_destroy(void *data)
{
   rib_rmlui_menu_t *menu = (rib_rmlui_menu_t*)data;
   if (rib_rmlui_active_menu == data)
      rib_rmlui_active_menu = NULL;
   if (menu && menu->capture_active)
      rib_rmlui_cancel_capture(menu, NULL);
   rib_rmlui_shutdown();
   pending_overlay_start = false;
   delete menu;
   /* We leave the small userdata wrapper of the C adapter to menu_driver_ctl. */
}

void rib_menu_context_destroy(void *data)
{
   rib_rmlui_menu_t *menu = (rib_rmlui_menu_t*)data;
   if (menu && menu->capture_active)
      rib_rmlui_cancel_capture(menu, NULL);
   rib_rmlui_shutdown();
   if (menu)
      menu->initialized = false;
}

void rib_menu_context_reset(void *data)
{
   rib_rmlui_menu_t *menu = (rib_rmlui_menu_t*)data;
   if (menu)
      menu->initialized = false;
   /* With a new video driver the menu is switched off, so for anything still
    * drawn over the game we ask for frames again. This happens, for example,
    * when the player goes fullscreen during an overlay. */
   if (menu && menu->overlays.drawing())
      rib_host_overlay_frames(true);
}

static bool rib_same_bind_target(const rib_rmlui_menu_t *menu, int left, int right)
{
   const char *group_left;
   const char *group_right;
   if (left == right)
      return true;
   if (!menu || left < 0 || right < 0
         || left >= menu->control_count || right >= menu->control_count)
      return false;
   group_left = menu->controls[left].group;
   group_right = menu->controls[right].group;
   return group_left[0] && group_right[0] && string_is_equal(group_left, group_right);
}

static void rib_bind_anchor(const rib_rmlui_menu_t *menu, int index,
      char *out, size_t length)
{
   if (menu->controls[index].group[0])
   {
      snprintf(out, length, "control-group-%s", menu->controls[index].group);
      if (rib_rmlui_has_element(out))
         return;
   }
   snprintf(out, length, "control-%s", menu->controls[index].id);
   if (rib_rmlui_has_element(out))
      return;
   snprintf(out, length, "control-hit-%s", menu->controls[index].id);
}

static void rib_hide_binds(void);

static void rib_callout_text(const rib_rmlui_menu_t *menu, int index,
      char *out, size_t length)
{
   int members[RIB_CONTROL_MAX];
   int member_count = 0;
   char details[RIB_HOST_BIND_LINE_MAX][64];
   char kinds[RIB_HOST_BIND_LINE_MAX][8];
   int lines = 0;
   int slot;
   size_t used = 0;

   if (!out || !length)
      return;
   out[0] = '\0';
   if (!menu || index < 0 || index >= menu->control_count)
   {
      strlcpy(out, "---", length);
      return;
   }
   if (menu->controls[index].group[0])
   {
      int cursor;
      for (cursor = 0; cursor < menu->control_count; ++cursor)
         if (rib_control_is_active(menu, cursor)
               && string_is_equal(menu->controls[cursor].group,
                     menu->controls[index].group))
            members[member_count++] = cursor;
   }
   else
      members[member_count++] = index;

   for (slot = 0; slot < member_count; ++slot)
   {
      const unsigned at = menu->controls[members[slot]].bind_index;
      rib_host_bind_lines(at,
            details, kinds, &lines);
   }
   if (lines <= 0)
   {
      strlcpy(out, "---", length);
      return;
   }
   for (slot = 0; slot < lines; ++slot)
   {
      if (slot && used + 2 < length)
      {
         out[used++] = ',';
         out[used++] = ' ';
         out[used] = '\0';
      }
      used += strlcpy(out + used, details[slot], length - used);
      if (used >= length)
         break;
   }
}

static void rib_show_binds(rib_rmlui_menu_t *menu, int index)
{
   int members[RIB_CONTROL_MAX];
   int member_count = 0;
   char details[RIB_HOST_BIND_LINE_MAX][64];
   char kinds[RIB_HOST_BIND_LINE_MAX][8];
   char titles[RIB_HOST_BIND_LINE_MAX][NAME_MAX_LENGTH];
   int lines = 0;
   int rows;
   int slot;
   int member;
   char anchor[96];

   if (menu->controls[index].group[0])
   {
      int cursor;
      for (cursor = 0; cursor < menu->control_count; ++cursor)
         if (rib_control_is_active(menu, cursor)
               && string_is_equal(menu->controls[cursor].group,
                     menu->controls[index].group))
            members[member_count++] = cursor;
   }
   else
      members[member_count++] = index;

   for (member = 0; member < member_count; ++member)
   {
      int before = lines;
      const unsigned at = menu->controls[members[member]].bind_index;
      rib_host_bind_lines(at,
            details, kinds, &lines);
      for (slot = before; slot < lines; ++slot)
      {
         const char *label = menu->control_labels[members[member]];
         if (!label[0])
            label = menu->controls[members[member]].id;
         strlcpy(titles[slot], label, sizeof(titles[slot]));
      }
   }

   /* With one binding, the callout already shows it. A list that repeated it
    * would open on every control as the player moves across the pad. */
   if (lines < 2)
   {
      rib_hide_binds();
      return;
   }

   rows = rib_rmlui_rows_in(rib_binds_list);
   if (lines > rows)
      RARCH_ERR("[RIB] '%s' has %d binds and the menu was built with %d rows; "
            "the rest are not shown.\n",
            menu->controls[index].id, lines, rows);
   for (slot = 0; slot < rows; ++slot)
   {
      const char *id = rib_rmlui_row_in(rib_binds_list, slot);
      char row[64];
      if (!id || !*id)
         break;
      strlcpy(row, id, sizeof(row));
      if (slot < lines)
      {
         rib_rmlui_set_row_text(row, titles[slot], details[slot], kinds[slot]);
         rib_rmlui_set_shown(row, true);
      }
      else
         rib_rmlui_set_shown(row, false);
   }
   rib_rmlui_retarget_pages(rib_binds_list);
   rib_bind_anchor(menu, index, anchor, sizeof(anchor));
   rib_rmlui_place_list(rib_binds_list, anchor, rib_binds_width);
   rib_binds_open = true;
}

static void rib_hide_binds(void)
{
   if (rib_binds_list[0])
      rib_rmlui_set_shown(rib_binds_list, false);
   rib_binds_open = false;
}

/* The one place where we set which control is current, as for the pause row.
 *
 * There is one selection for the keys and the pointer. As in the pause row,
 * we write the index from the pointer and read it for the keys, so only one
 * control looks selected. We play no scroll sound for the pointer, because
 * moving the pointer is not a key press. */
static void rib_focus_control(rib_rmlui_menu_t *menu, int index)
{
   if (!menu || index == menu->control_focus)
      return;
   if (index < 0 || index > RIB_CONTROL_MAX + 1)
      return;
   if (index < RIB_CONTROL_MAX && !rib_control_is_active(menu, index))
      return;
   menu->control_focus = index;
   if (index < RIB_CONTROL_MAX)
      menu->selected_control = index;
   rib_rmlui_refresh_controls(menu);
}

static void rib_rmlui_update_binds(rib_rmlui_menu_t *menu, int x, int y,
      bool pointer_active, bool hover_active)
{
   int current = -1;
   int hovered;
   int64_t now;

   if (!menu || !rib_binds_list[0] || !menu->controls_visible
         || menu->capture_active || menu->device_picker_open)
   {
      rib_hide_binds();
      rib_binds_for = -1;
      return;
   }

   if (pointer_active
         && rib_rmlui_pointer_inside(rib_binds_list, x, y)
         && rib_binds_for >= 0)
      current = rib_binds_for;
   else if (hover_active)
   {
      hovered = rib_rmlui_hovered_action();
      if (hovered >= RIB_RMLUI_ACTION_CONTROL_FIRST
            && hovered <= RIB_RMLUI_ACTION_CONTROL_LAST)
      {
         const int index = hovered - RIB_RMLUI_ACTION_CONTROL_FIRST;
         if (rib_control_is_active(menu, index))
         {
            rib_focus_control(menu, index);
            current = index;
         }
      }
      else if (hovered == RIB_RMLUI_ACTION_CONTROLS_RESET
            || hovered == RIB_RMLUI_ACTION_CONTROLS_BACK)
      {
         rib_focus_control(menu, hovered == RIB_RMLUI_ACTION_CONTROLS_RESET
               ? RIB_CONTROL_MAX : RIB_CONTROL_MAX + 1);
         current = -1;
      }
   }
   if (current < 0 && menu->control_focus >= 0
         && menu->control_focus < menu->control_count)
      current = menu->control_focus;
   if (!rib_control_is_active(menu, current))
      current = -1;

   if (!rib_same_bind_target(menu, current, rib_binds_for))
   {
      rib_hide_binds();
      rib_binds_for = current;
      rib_binds_since = rib_host_time_us();
   }
   if (current < 0 || rib_binds_open)
      return;
   now = rib_host_time_us();
   if (now - rib_binds_since >= (int64_t)rib_binds_after_ms * 1000)
      rib_show_binds(menu, current);
}

void rib_menu_frame(void *data, int width, int height)
{
   rib_rmlui_menu_t *menu = (rib_rmlui_menu_t*)data;
   rib_pointer pointer;
   const char *asset_directory = getenv("ROMINABOX_RML_ASSETS");
   const char *data_directory = rib_absolute_data_dir();

   if (!menu)
      return;

   if (!menu->initialized)
   {
      if (!asset_directory || !*asset_directory)
         asset_directory = RIB_RMLUI_DEFAULT_ASSETS;
      menu->initialized = rib_rmlui_init(asset_directory,
            width, height,
            rib_host_core_gl_context());
      if (!menu->initialized)
      {
         RARCH_ERR("[RmlUi] Failed to initialize menu from %s.\n",
               asset_directory);
         menu->overlays.stop();
         rib_host_overlay_frames(false);
         return;
      }
      /* Read the screens and overlays in the design before we show any. */
      rib_rmlui_load_design(menu, asset_directory);
      rib_rmlui_load_shaders(menu, asset_directory);
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
      rib_focus_control(menu, rib_control_first(menu));
      rib_rmlui_refresh_controls(menu);
      rib_rmlui_set_slider_step(RIB_VOLUME_SLIDER_ID,
            AUDIO_VOLUME_STEP_DB
            / (AUDIO_VOLUME_MAX_DB - AUDIO_VOLUME_MIN_DB));
      /* A file may have mute on, or a level in decibels above the top. We use
       * the quiet end in place of mute and clamp anything above normal to
       * normal. Write the file again only when that changes its contents. */
      {
         float db = rib_host_volume();
         bool muted = rib_host_muted();
         float snapped = rib_volume_quantize_db(muted ? AUDIO_VOLUME_MIN_DB : db);
         rib_set_volume_db(menu, snapped, muted || snapped != db);
      }
      /* After the slots, so the lock from a switch replaces the slot count. */
      rib_rmlui_apply_toggles(menu);
      RARCH_LOG("[RmlUi] Loaded menu from %s.\n", asset_directory);
   }

   {
      const bool menu_alive = rib_host_menu_open();

      /* When we draw this document while the menu is closed, the menu itself
       * is not on screen, and we state that on the document for the design.
       * We set it here and not with the overlays, because we may still draw
       * the document after the last overlay has gone. */
      if (menu->overlay_mode != !menu_alive)
      {
         menu->overlay_mode = !menu_alive;
         rib_rmlui_set_overlay_mode(menu->overlay_mode);
      }

      if (!menu_alive)
      {
         /* The game is running and the player is playing it with the
          * controller. Nothing below applies now: no pointer, no queued
          * actions and no capture. We have a frame in this driver only to
          * draw over the game.
          *
          * We run the script first, because from it we learn whether we still
          * want frames after the overlays are done, and we can learn that only
          * after asking it. */
         menu->script.run(menu, {menu->screen, menu->transfer_pending,
               menu->capture_active, menu->profile_id});
         menu->overlays.update(menu->script.wants_frames());
         rib_rmlui_render(width, height);
         return;
      }
      menu->overlays.update(menu->script.wants_frames());
   }

   pointer = rib_host_pointer();
   {
      bool pointer_pressed =
            pointer.pressed;

      rib_rmlui_pointer_move(pointer.x, pointer.y);
      menu->script.restore_hover();
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
    * in the same loop as a click from the player. We put back a hover from
    * the script after the click, because the pointer move above followed the
    * mouse and would have removed the hover. The disc entry starts hidden.
    * Fill it before the click from the script, or the click goes to a button
    * that is still display:none in the document. */
   rib_discs_sync();
   menu->script.run(menu, {menu->screen, menu->transfer_pending,
               menu->capture_active, menu->profile_id});
   menu->script.restore_hover();
   /* After we put the pointer back for the script, so a hovered row is the
    * focused row before the click in this frame. We play no scroll sound,
    * because the pointer did not move by a step. */
   if (!menu->capture_active)
      rib_focus_list(menu, rib_rmlui_hovered_list_row());

   for (;;)
   {
      auto next_action = rib_rmlui_take_event();
      if (next_action.kind == RIB_RMLUI_ACTION_NONE)
         break;
      rib_rmlui_perform_action(menu, next_action);
   }

   if (menu->capture_active)
   {
      char capture_status[96];
      float remaining = 0.0f;
      enum rib_capture_result result = rib_host_capture_poll(
            !menu->capture_ignore_pointer, &remaining);
      if (result == RIB_CAPTURE_CAPTURED)
      {
         int conflict = rib_rmlui_find_binding_conflict(
               menu, menu->capture_control);
         menu->capture_active = false;
         rib_host_restore_keyboard_mapping();
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
      else if (result == RIB_CAPTURE_TIMED_OUT)
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
   rib_rmlui_update_binds(menu, pointer.x, pointer.y,
         !menu->script.wants_frames(),
         !menu->script.wants_frames() || menu->script.has_hover());
   rib_rmlui_render(width, height);

}

/* A screen other than Pause and Controls, whose navigation we leave as it
 * was. On any other screen, the player moves through the parts of the panel in
 * the design, such as a slider, a toggle or a button, in document order. */
static int rib_part_navigate(rib_rmlui_menu_t *menu, enum rib_key action)
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
      case RIB_KEY_UP:
         menu->panel_focus = (menu->panel_focus + count - 1) % count;
         rib_rmlui_mark_focused(panel, ids[menu->panel_focus]);
#ifdef HAVE_AUDIOMIXER
         rib_host_scroll_sound(true);
#endif
         return 0;
      case RIB_KEY_DOWN:
         menu->panel_focus = (menu->panel_focus + 1) % count;
         rib_rmlui_mark_focused(panel, ids[menu->panel_focus]);
#ifdef HAVE_AUDIOMIXER
         rib_host_scroll_sound(false);
#endif
         return 0;
      case RIB_KEY_LEFT:
      case RIB_KEY_RIGHT:
         if (rib_rmlui_part_is_slider(ids[menu->panel_focus]))
            rib_rmlui_nudge_slider(ids[menu->panel_focus],
                  action == RIB_KEY_RIGHT ? 1 : -1);
         return 0;
      case RIB_KEY_OK:
      case RIB_KEY_SELECT:
         rib_rmlui_click_element(ids[menu->panel_focus]);
         return 0;
      case RIB_KEY_CANCEL:
      case RIB_KEY_RESUME:
      case RIB_KEY_TOGGLE:
         rib_rmlui_play_action_sound(RIB_RMLUI_ACTION_CONTROLS_BACK);
         strlcpy(menu->screen, "pause", sizeof(menu->screen));
         rib_rmlui_show_screen("pause");
         rib_rmlui_mark_focused(panel, NULL);
         return 0;
      default:
         return 0;
   }
}

int rib_menu_key(void *data, enum rib_key action)
{
   rib_rmlui_menu_t *menu = (rib_rmlui_menu_t*)data;

   if (!menu)
      return 0;

   if (!string_is_equal(menu->screen, "pause") && !menu->controls_visible)
   {
      char ids[16][64];
      const char *panel = rib_rmlui_screen_panel(menu->screen);
      int parts = rib_rmlui_focusables(panel, ids, 16);
      int rows = rib_rmlui_visible_row_count();
      bool slider = false;
      int part;

      for (part = 0; part < parts; ++part)
         if (rib_rmlui_part_is_slider(ids[part]))
            slider = true;
      /* Left and right move a slider. They page a list only when the panel
       * has no slider, which is how a shader list turns its pages. */
      if (slider)
         return rib_part_navigate(menu, action);
      if (rows <= 0)
         return rib_part_navigate(menu, action);

      const int stops = rows + rib_rmlui_list_control_count();

      switch (action)
      {
         case RIB_KEY_UP:
            if (stops > 0)
            {
               rib_focus_list(menu, (menu->list_focus + stops - 1) % stops);
#ifdef HAVE_AUDIOMIXER
               rib_host_scroll_sound(true);
#endif
            }
            return 0;
         case RIB_KEY_DOWN:
            if (stops > 0)
            {
               rib_focus_list(menu, (menu->list_focus + 1) % stops);
#ifdef HAVE_AUDIOMIXER
               rib_host_scroll_sound(false);
#endif
            }
            return 0;
         case RIB_KEY_LEFT:
            if (rib_rmlui_turn_list_page(-1) >= 0)
            {
               rib_focus_list(menu, 0);
               rib_rmlui_play_action_sound(RIB_RMLUI_ACTION_LIST_PAGE);
            }
            return 0;
         case RIB_KEY_RIGHT:
            if (rib_rmlui_turn_list_page(1) >= 0)
            {
               rib_focus_list(menu, 0);
               rib_rmlui_play_action_sound(RIB_RMLUI_ACTION_LIST_PAGE);
            }
            return 0;
         case RIB_KEY_OK:
         case RIB_KEY_SELECT:
            if (menu->list_focus >= rows)
            {
               /* Through the listener on the element, the same path as for a
                * pointer, which already has the code for the switch and BACK. */
               rib_rmlui_click_element(
                     rib_rmlui_list_control_id(menu->list_focus - rows));
            }
            else if (rows > 0)
            {
               rib_rmlui_perform_action(menu, {RIB_RMLUI_ACTION_LIST_CHOOSE,
                     rib_rmlui_list_row_id(menu->list_focus)});
            }
            return 0;
         case RIB_KEY_CANCEL:
         case RIB_KEY_RESUME:
         case RIB_KEY_TOGGLE:
            /* We leave this screen through its back button, so pressing Escape
             * goes to the same place as BACK, including back to Options from a
             * screen opened from Options. */
            if (!rib_rmlui_click_screen_back())
               rib_rmlui_perform_action(menu, rib_rmlui_map_menu_toggle(
                     true, false));
            return 0;
         default:
            return 0;
      }
   }

   if (menu->controls_visible)
   {
      if (menu->capture_active)
      {
         if (action == RIB_KEY_CANCEL || action == RIB_KEY_RESUME ||
             action == RIB_KEY_TOGGLE)
            rib_rmlui_perform_action(menu, RIB_RMLUI_ACTION_CONTROLS_CANCEL);
         return 0;
      }

      switch (action)
      {
         case RIB_KEY_UP:
         case RIB_KEY_LEFT:
#ifdef HAVE_AUDIOMIXER
            rib_host_scroll_sound(true);
#endif
            rib_focus_control(menu, rib_control_step(menu,
                  menu->control_focus, -1));
            return 0;
         case RIB_KEY_DOWN:
         case RIB_KEY_RIGHT:
#ifdef HAVE_AUDIOMIXER
            rib_host_scroll_sound(false);
#endif
            rib_focus_control(menu, rib_control_step(menu,
                  menu->control_focus, 1));
            return 0;
         case RIB_KEY_OK:
         case RIB_KEY_SELECT:
            if (menu->control_focus < RIB_CONTROL_MAX)
               rib_rmlui_start_capture(menu, menu->control_focus);
            else if (menu->control_focus == RIB_CONTROL_MAX)
               rib_rmlui_perform_action(menu, RIB_RMLUI_ACTION_CONTROLS_RESET);
            else
               rib_rmlui_perform_action(menu, RIB_RMLUI_ACTION_CONTROLS_BACK);
            return 0;
         case RIB_KEY_START:
            rib_rmlui_perform_action(menu, RIB_RMLUI_ACTION_CONTROLS_RESET);
            return 0;
         case RIB_KEY_CANCEL:
         case RIB_KEY_RESUME:
         case RIB_KEY_TOGGLE:
            /* We leave this screen through its back button, so pressing Escape
             * goes to the same place as BACK, including back to Options from a
             * screen opened from Options. */
            if (!rib_rmlui_click_screen_back())
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
      case RIB_KEY_UP:
         if (menu->row_focus < 0)
         {
            int slot = rib_rmlui_focus_slot(menu->focused);
            if (slot > 3)
               rib_rmlui_focus(menu, menu->focused - 3, true);
            else
            {
               /* From the top row of slots to the buttons, in the column of
                * the slot. When the row has fewer than three buttons, we
                * focus its last button. */
               char ids[16][64];
               const int count = rib_pause_row(ids, 16);
               rib_pause_focus_row(menu,
                     slot - 1 < count ? slot - 1 : count - 1, true);
            }
         }
         else
         {
            const int column = menu->row_focus > 2 ? 2 : menu->row_focus;
            menu->row_focus = -1;
            rib_rmlui_focus(menu,
                  RIB_RMLUI_ACTION_SELECT_SLOT_1 + 3 + column, true);
         }
         return 0;
      case RIB_KEY_DOWN:
         if (menu->row_focus < 0)
         {
            int slot = rib_rmlui_focus_slot(menu->focused);
            if (slot <= 3)
               rib_rmlui_focus(menu, menu->focused + 3, false);
            else
            {
               char ids[16][64];
               const int count = rib_pause_row(ids, 16);
               rib_pause_focus_row(menu,
                     slot - 4 < count ? slot - 4 : count - 1, false);
            }
         }
         else
         {
            const int column = menu->row_focus > 2 ? 2 : menu->row_focus;
            menu->row_focus = -1;
            rib_rmlui_focus(menu,
                  RIB_RMLUI_ACTION_SELECT_SLOT_1 + column, false);
         }
         return 0;
      case RIB_KEY_LEFT:
         if (menu->row_focus < 0)
         {
            int slot = rib_rmlui_focus_slot(menu->focused);
            int row_start = slot <= 3 ? 1 : 4;
            slot = slot == row_start ? row_start + 2 : slot - 1;
            rib_rmlui_focus(menu,
                  RIB_RMLUI_ACTION_SELECT_SLOT_1 + slot - 1, true);
         }
         else
         {
            char ids[16][64];
            const int count = rib_pause_row(ids, 16);
            rib_pause_focus_row(menu,
                  rib_pause_row_index((const char (*)[64])ids, count) - 1, true);
         }
         return 0;
      case RIB_KEY_RIGHT:
         if (menu->row_focus < 0)
         {
            int slot = rib_rmlui_focus_slot(menu->focused);
            int row_end = slot <= 3 ? 3 : 6;
            slot = slot == row_end ? row_end - 2 : slot + 1;
            rib_rmlui_focus(menu,
                  RIB_RMLUI_ACTION_SELECT_SLOT_1 + slot - 1, false);
         }
         else
         {
            char ids[16][64];
            const int count = rib_pause_row(ids, 16);
            rib_pause_focus_row(menu,
                  rib_pause_row_index((const char (*)[64])ids, count) + 1, false);
         }
         return 0;
      case RIB_KEY_OK:
      case RIB_KEY_SELECT:
         if (menu->row_focus < 0)
            rib_rmlui_perform_action(menu, static_cast<rib_rmlui_action>(menu->focused));
         else
         {
            /* Press the element itself, so a button from the design opens what
             * its listener opens, without its name in this code. */
            char ids[16][64];
            const int count = rib_pause_row(ids, 16);
            const int index =
                  rib_pause_row_index((const char (*)[64])ids, count);
            if (count > 0)
               rib_rmlui_click_element(ids[index]);
         }
         return 0;
      case RIB_KEY_CANCEL:
      case RIB_KEY_RESUME:
      case RIB_KEY_TOGGLE:
         rib_rmlui_perform_action(menu, rib_rmlui_map_menu_toggle(
               false, false));
         return 0;
      case RIB_KEY_START:
         rib_rmlui_perform_action(menu, RIB_RMLUI_ACTION_SAVE);
         return 0;
      default:
         return 0;
   }
}
