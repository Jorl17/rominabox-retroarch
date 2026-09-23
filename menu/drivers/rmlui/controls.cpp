#include "controls.hpp"
#include "host.h"
#include "files.h"
#include "../rmlui_bridge.h"
#include "../../../verbosity.h"
#include <file/config_file.h>
#include <streams/file_stream.h>
#include <string/stdstring.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifndef RIB_RMLUI_DEFAULT_ASSETS
#define RIB_RMLUI_DEFAULT_ASSETS "."
#endif

namespace rib {
static bool control_belongs(const char *list, const char *id)
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

void Controls::discover_controls(config_file_t *config)
{
   struct config_file_entry entry;
   bool present;
   char key[96];
   char belonging[1024];

   count = 0;
   belonging[0] = '\0';
   if (profile_id[0])
   {
      snprintf(key, sizeof(key), "controls_variant_controls_%s",
            profile_id);
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
      if (!control_belongs(belonging, id))
         continue;
      if (!rib_host_bind_index(id, &bind_index))
      {
         RARCH_WARN("[RIB] '%s' is not a libretro bind; the menu will not show "
               "it. Check the id against DECLARE_BIND in configuration.c.\n", id);
         continue;
      }
      if (count >= RIB_CONTROL_MAX)
      {
         /* We log this, so that a control left out, such as a DualShock
          * stick, appears in the log. */
         RARCH_ERR("[RIB] more than %d controls declared; '%s' and anything "
               "after it are unreachable.\n", RIB_CONTROL_MAX, id);
         return;
      }
      strlcpy(entries[count].id, id,
            sizeof(entries[count].id));
      entries[count].group[0] = '\0';
      {
         char group_key[96];
         snprintf(group_key, sizeof(group_key), "rib_group_%s", id);
         config_get_array(config, group_key,
               entries[count].group,
               sizeof(entries[count].group));
      }
      entries[count].bind_index = bind_index;
      ++count;
   }
}

void Controls::discover_devices(config_file_t *config)
{
   char list[512];
   char *cursor;
   char *token;

   device_count = 0;
   if (!config_get_array(config, "controls_variants", list, sizeof(list)))
      return;

   cursor = list;
   while ((token = strtok_r(cursor, " ", &cursor)))
   {
      char key[96];
      char name[NAME_MAX_LENGTH];

      if (!*token)
         continue;
      if (device_count >= RIB_DEVICE_MAX)
      {
         RARCH_ERR("[RIB] more than %d controllers offered; '%s' and any after "
               "it cannot be chosen.\n", RIB_DEVICE_MAX, token);
         return;
      }
      strlcpy(device_ids[device_count], token,
            sizeof(device_ids[device_count]));
      snprintf(key, sizeof(key), "controls_variant_device_%s", token);
      device_libretro[device_count] = 0;
      {
         char device[32];
         if (config_get_array(config, key, device, sizeof(device)))
            device_libretro[device_count] =
               (unsigned)strtoul(device, NULL, 10);
      }
      snprintf(key, sizeof(key), "controls_variant_name_%s", token);
      if (config_get_array(config, key, name, sizeof(name)))
         strlcpy(device_names[device_count], name,
               sizeof(device_names[device_count]));
      else
         strlcpy(device_names[device_count], token,
               sizeof(device_names[device_count]));
      ++device_count;
   }
}

int Controls::index_of(const char *id) const
{
   for (int index = 0; id && index < count; ++index)
      if (string_is_equal(entries[index].id, id)) return index;
   return -1;
}

bool Controls::active(int index) const
{
   return index >= 0 && index < count &&
          enabled[index];
}

const char * Controls::console_name(int index) const
{
   if (labels[index][0])
      return labels[index];
   return entries[index].id;
}

int Controls::first() const
{
   int index;
   for (index = 0; index < count; ++index)
      if (active(index))
         return index;
   return 0;
}

int Controls::step(int current, int direction) const
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
   for (index = 0; index < count; ++index)
      if (active(index))
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

bool Controls::load_file(const char *path, bool defaults)
{
   config_file_t *config;
   char profile[32] = {0};
   int index;

   if (!path || !(config = config_file_new_from_path_to_string(path)))
      return false;

   /* The starting pad is in the author's defaults. We load the per-game
    * override after them. It contains the pad the player chose later, so we
    * use that pad, and the player sees it in the picker at every launch. We
    * read the list of variants only from the defaults, because the override
    * does not contain it. */
   if (config_get_array(config, "controls_profile", profile, sizeof(profile))
         && profile[0])
      strlcpy(profile_id, profile, sizeof(profile_id));

   if (defaults)
   {
      discover_controls(config);
      discover_devices(config);
      /* We load the document before we build these lists, so there are no
       * listeners yet on its control and picker elements. */
      rib_rmlui_wire_controls();
      rib_rmlui_wire_device_picker();
      rib_rmlui_set_device_picker(false, profile_id);
   }
   else if (profile[0])
      rib_rmlui_set_device_picker(false, profile_id);

   if (defaults)
      for (index = 0; index < count; ++index)
      {
         char key[96];
         const char *suffixes[] = {"", "_btn", "_axis", "_mbtn"};
         unsigned suffix_index;
         enabled[index] = false;
         for (suffix_index = 0; suffix_index < ARRAY_SIZE(suffixes);
              ++suffix_index)
         {
            snprintf(key, sizeof(key), "input_player1_%s%s",
                  entries[index].id, suffixes[suffix_index]);
            if (config_get_entry(config, key))
            {
               enabled[index] = true;
               break;
            }
         }
      }

   for (index = 0; index < count; ++index)
   {
      char key[64];
      char label[NAME_MAX_LENGTH] = {0};

      if (!active(index))
         continue;
      if (defaults)
      {
         labels[index][0] = '\0';
         rib_host_clear_bind(entries[index].bind_index);
      }
      snprintf(key, sizeof(key), "rib_label_%s", entries[index].id);
      if (config_get_array(config, key, label, sizeof(label)))
         strlcpy(labels[index], label,
               sizeof(labels[index]));
      rib_host_load_bind(config, entries[index].id, entries[index].bind_index);

   }
   rib_host_restore_keyboard_mapping();
   config_file_free(config);
   return true;
}

void Controls::reload()
{
   const char *asset_directory = getenv("ROMINABOX_RML_ASSETS");
   char defaults_path[PATH_MAX_LENGTH];

   if (!asset_directory || !*asset_directory)
      asset_directory = RIB_RMLUI_DEFAULT_ASSETS;
   snprintf(defaults_path, sizeof(defaults_path),
         "%s/controls-defaults.cfg", asset_directory);

   focused = 0;
   if (!load_file(defaults_path, true))
   {
      RARCH_ERR("[RIB] could not re-read controls from %s after changing "
            "controller; the menu still lists the previous pad.\n",
            defaults_path);
      return;
   }
   if (path[0])
      load_file(path, false);
   rib_rmlui_wire_controls();
   focus(first());
   refresh();
}

bool Controls::save()
{
   config_file_t *config;
   bool saved;
   int index;

   if (!path[0] || !(config = config_file_new_alloc()))
      return false;

   config_set_string(config, "controls_profile", profile_id);
   for (index = 0; index < count; ++index)
   {
      char key[96];

      if (!active(index))
         continue;
      snprintf(key, sizeof(key), "rib_label_%s", entries[index].id);
      config_set_string(config, key, labels[index]);

      rib_host_write_bind(config, entries[index].id, entries[index].bind_index);
   }

   saved = rib_write_menu_config(config, path,
         RIB_CONFIG_WRITE_CONTROLS);
   config_file_free(config);
   return saved;
}

void Controls::refresh()
{
   int index;
   for (index = 0; index < count; ++index)
   {
      char display_label[NAME_MAX_LENGTH * 2];
      char binding[4096];
      char group_id[96];

      if (!active(index))
         continue;
      strlcpy(display_label, labels[index],
            sizeof(display_label));
      callout_text(index, binding, sizeof(binding));
      rib_rmlui_set_control_state(entries[index].id,
            display_label, binding,
            visible && focused == index,
            capture_active && capture_control == index);
      if (entries[index].group[0])
      {
         snprintf(group_id, sizeof(group_id), "control-group-binding-%s",
               entries[index].group);
         rib_rmlui_set_element_text(group_id, binding);
      }
   }
   rib_rmlui_set_controls_action_focus(
         visible && focused == RIB_CONTROL_MAX,
         visible && focused == RIB_CONTROL_MAX + 1,
         capture_active);
   if (visible && focused >= 0
         && focused < count
         && entries[focused].group[0])
      rib_rmlui_focus_group(entries[focused].group);
   else
      rib_rmlui_focus_group(NULL);
}

void Controls::cancel_capture(const char *status)
{
   if (!capture_active)
      return;
   rib_host_capture_cancel();
   capture_active = false;
   rib_rmlui_set_controls_status(status ? status : "BINDING UNCHANGED");
   rib_rmlui_set_footer_hint(visible ? "ESC  BACK" :
                                                       "ESC  CONTINUE");
   refresh();
}

void Controls::start_capture(int index)
{
   char status[96];
   if (!active(index))
      return;
   if (!rib_host_capture_start(entries[index].bind_index,
            RIB_CONTROL_CAPTURE_SECONDS))
   {
      rib_rmlui_set_controls_status("CAPTURE COULD NOT START");
      return;
   }
   capture_active = true;
   capture_control = index;
   capture_ignore_pointer = true;
   snprintf(status, sizeof(status), "%s: PRESS AN INPUT (10)",
         console_name(index));
   rib_rmlui_set_controls_status(status);
   rib_rmlui_set_footer_hint("ESC  CANCEL");
   refresh();
}

int Controls::find_conflict(int changed_index) const
{
   int index;
   if (!active(changed_index))
      return -1;
   for (index = 0; index < count; ++index)
   {
      if (index == changed_index || !active(index))
         continue;
      if (rib_host_bind_conflicts(entries[changed_index].bind_index,
               entries[index].bind_index))
         return index;
   }
   return -1;
}

bool Controls::same_bind_target(int left, int right) const
{
   const char *group_left;
   const char *group_right;
   if (left == right)
      return true;
   if (left < 0 || right < 0
         || left >= count || right >= count)
      return false;
   group_left = entries[left].group;
   group_right = entries[right].group;
   return group_left[0] && group_right[0] && string_is_equal(group_left, group_right);
}

void Controls::bind_anchor(int index, char *out, size_t length) const
{
   if (entries[index].group[0])
   {
      snprintf(out, length, "control-group-%s", entries[index].group);
      if (rib_rmlui_has_element(out))
         return;
   }
   snprintf(out, length, "control-%s", entries[index].id);
   if (rib_rmlui_has_element(out))
      return;
   snprintf(out, length, "control-hit-%s", entries[index].id);
}

void Controls::callout_text(int index, char *out, size_t length) const
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
   if (index < 0 || index >= count)
   {
      strlcpy(out, "---", length);
      return;
   }
   member_count = bind_members(index, members);

   for (slot = 0; slot < member_count; ++slot)
   {
      const unsigned at = entries[members[slot]].bind_index;
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

void Controls::show_binds(int index)
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

   member_count = bind_members(index, members);

   for (member = 0; member < member_count; ++member)
   {
      int before = lines;
      const unsigned at = entries[members[member]].bind_index;
      rib_host_bind_lines(at,
            details, kinds, &lines);
      for (slot = before; slot < lines; ++slot)
      {
         const char *label = labels[members[member]];
         if (!label[0])
            label = entries[members[member]].id;
         strlcpy(titles[slot], label, sizeof(titles[slot]));
      }
   }

   /* With one binding, the callout already shows it. A list that repeated it
    * would open on every control as the player moves across the pad. */
   if (lines < 2)
   {
      hide_binds();
      return;
   }

   rows = rib_rmlui_rows_in(binds.list);
   if (lines > rows)
      RARCH_ERR("[RIB] '%s' has %d binds and the menu was built with %d rows; "
            "the rest are not shown.\n",
            entries[index].id, lines, rows);
   for (slot = 0; slot < rows; ++slot)
   {
      const char *id = rib_rmlui_row_in(binds.list, slot);
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
   rib_rmlui_retarget_pages(binds.list);
   bind_anchor(index, anchor, sizeof(anchor));
   rib_rmlui_place_list(binds.list, anchor, binds.width);
   binds.open = true;
}

void Controls::hide_binds()
{
   if (binds.list[0])
      rib_rmlui_set_shown(binds.list, false);
   binds.open = false;
}

void Controls::focus(int index)
{
   if (index == focused)
      return;
   if (index < 0 || index > RIB_CONTROL_MAX + 1)
      return;
   if (index < RIB_CONTROL_MAX && !active(index))
      return;
   focused = index;
   refresh();
}

void Controls::update_binds(int x, int y, bool pointer_active, bool hover_active)
{
   int current = -1;
   rib::Event hovered;
   int64_t now;

   if (!binds.list[0] || !visible
         || capture_active || device_picker_open)
   {
      hide_binds();
      binds.control = -1;
      return;
   }

   if (pointer_active
         && rib_rmlui_pointer_inside(binds.list, x, y)
         && binds.control >= 0)
      current = binds.control;
   else if (hover_active)
   {
      hovered = rib_rmlui_hovered_event();
      if (hovered.kind == RIB_RMLUI_ACTION_CONTROL)
      {
         const int index = index_of(hovered.id.c_str());
         if (active(index))
         {
            focus(index);
            current = index;
         }
      }
      else if (hovered.kind == RIB_RMLUI_ACTION_CONTROLS_RESET
            || hovered.kind == RIB_RMLUI_ACTION_CONTROLS_BACK)
      {
         focus(hovered.kind == RIB_RMLUI_ACTION_CONTROLS_RESET
               ? RIB_CONTROL_MAX : RIB_CONTROL_MAX + 1);
         current = -1;
      }
   }
   if (current < 0 && focused >= 0
         && focused < count)
      current = focused;
   if (!active(current))
      current = -1;

   if (!same_bind_target(current, binds.control))
   {
      hide_binds();
      binds.control = current;
      binds.since = rib_host_time_us();
   }
   if (current < 0 || binds.open)
      return;
   now = rib_host_time_us();
   if (now - binds.since >= (int64_t)binds.after_ms * 1000)
      show_binds(current);
}

void Controls::choose_device(const char *chosen)
{
   device_picker_open = false;
   if (chosen && *chosen && !string_is_equal(chosen, profile_id))
   {
      int index;
      unsigned device = 0;
      bool known = false;

      strlcpy(profile_id, chosen, sizeof(profile_id));
      /* The pad belongs to the player who picks it, so we write it to the
       * per-game override and never to the author's fixed defaults. */
      save();

      for (index = 0; index < device_count; ++index)
         if (string_is_equal(device_ids[index], chosen))
         {
            device = device_libretro[index];
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
                  reload();
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
   rib_rmlui_set_device_picker(false, profile_id);
}

void Controls::reset_defaults()
{
   char defaults_path[PATH_MAX_LENGTH];
   const char *asset_directory = getenv("ROMINABOX_RML_ASSETS");
   if (!asset_directory || !*asset_directory)
      asset_directory = RIB_RMLUI_DEFAULT_ASSETS;
   if (capture_active)
      cancel_capture(NULL);
   snprintf(defaults_path, sizeof(defaults_path),
         "%s/controls-defaults.cfg", asset_directory);
   if (!load_file(defaults_path, true))
      rib_rmlui_set_controls_status("DEFAULTS COULD NOT BE LOADED");
   else if (!save())
      rib_rmlui_set_controls_status("DEFAULTS RESTORED; SAVE FAILED");
   else
      rib_rmlui_set_controls_status("DEFAULTS RESTORED");
   refresh();
}

void Controls::poll_capture()
{
   if (capture_active)
   {
      char capture_status[96];
      float remaining = 0.0f;
      enum rib_capture_result result = rib_host_capture_poll(
            !capture_ignore_pointer, &remaining);
      if (result == RIB_CAPTURE_CAPTURED)
      {
         int conflict = find_conflict(capture_control);
         capture_active = false;
         rib_host_restore_keyboard_mapping();
         if (conflict >= 0)
         {
            snprintf(capture_status, sizeof(capture_status),
                  "SAVED; ALSO USED BY %s",
                  console_name(conflict));
            if (!save())
               strlcpy(capture_status, "BINDING ACTIVE; SAVE FAILED",
                     sizeof(capture_status));
            rib_rmlui_set_controls_status(capture_status);
         }
         else if (save())
            rib_rmlui_set_controls_status("BINDING SAVED");
         else
            rib_rmlui_set_controls_status("BINDING ACTIVE; SAVE FAILED");
         refresh();
         rib_rmlui_set_footer_hint("ESC  BACK");
      }
      else if (result == RIB_CAPTURE_TIMED_OUT)
      {
         capture_active = false;
         rib_rmlui_set_controls_status("TIMED OUT; BINDING UNCHANGED");
         rib_rmlui_set_footer_hint("ESC  BACK");
         refresh();
      }
      else
      {
         snprintf(capture_status, sizeof(capture_status),
               "%s: PRESS AN INPUT (%u)",
               console_name(capture_control),
               (unsigned)(remaining + 0.999f));
         rib_rmlui_set_controls_status(capture_status);
      }
   }

}

void Controls::configure_binds(const rib_design_data& design)
{
   strlcpy(binds.list, design.binds_list, sizeof(binds.list));
   binds.after_ms = design.binds_after_ms;
   binds.width = design.binds_width;
}

void Controls::toggle_picker()
{
   device_picker_open = !device_picker_open;
   rib_rmlui_set_device_picker(device_picker_open, profile_id);
}


int Controls::bind_members(int index, int *members) const
{
   if (!entries[index].group[0])
   {
      members[0] = index;
      return 1;
   }
   int found = 0;
   for (int cursor = 0; cursor < count; ++cursor)
      if (active(cursor) && string_is_equal(entries[cursor].group, entries[index].group))
         members[found++] = cursor;
   return found;
}

}
