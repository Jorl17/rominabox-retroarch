#include "words.hpp"
#include "controls.hpp"
#include "host.h"
#include "files.h"
#include "document.hpp"
#include "control_view.hpp"
#include "lists.hpp"
#include "status.hpp"
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
int Controls::index_of(const char *id) const
{
   for (int index = 0; id && index < catalog.count; ++index)
      if (string_is_equal(catalog.entries[index].id, id)) return index;
   return -1;
}

bool Controls::active(int index) const
{
   return index >= 0 && index < catalog.count &&
          catalog.entries[index].enabled;
}

const char * Controls::console_name(int index) const
{
   if (catalog.entries[index].label[0])
      return catalog.entries[index].label;
   return catalog.entries[index].id;
}

FocusTarget Controls::first() const
{
   for (int index = 0; index < catalog.count; ++index)
      if (active(index)) return FocusTarget::item(index);
   return FocusTarget::item(0);
}

FocusTarget Controls::step(int direction) const
{
   std::vector<FocusTarget> stops;
   for (int index = 0; index < catalog.count; ++index)
      if (active(index)) stops.push_back(FocusTarget::item(index));
   stops.push_back(FocusTarget::reset());
   stops.push_back(FocusTarget::back());
   return focus_state.next(FocusRegion::Controls, stops, direction);
}

bool Controls::load_file(const char *path, bool defaults)
{
   config_file_t *config;
   bool profile_present = false;
   int index;

   /* The starting pad is in the author's defaults. We load the per-game
    * override after them. It contains the pad the player chose later, so we
    * use that pad, and the player sees it in the picker at every launch. We
    * read the list of variants only from the defaults, because the override
    * does not contain it. */
   if (!(config = rib_open_controls(path, defaults, profile_id,
               &catalog, &profile_present, rib_host_bind_index)))
      return false;

   if (defaults)
   {
      /* We load the document before we build these lists, so there are no
       * listeners yet on its control and picker elements. */
      control_view.wire_controls(catalog);
      control_view.wire_device_picker(catalog);
      control_view.set_device_picker(catalog, false, profile_id);
   }
   else if (profile_present)
      control_view.set_device_picker(catalog, false, profile_id);

   if (defaults)
      rib_controls_read_enabled(config, &catalog);

   for (index = 0; index < catalog.count; ++index)
   {
      if (!active(index))
         continue;
      if (defaults)
      {
         catalog.entries[index].label[0] = '\0';
         rib_host_clear_bind(catalog.entries[index].bind_index);
      }
      rib_controls_read_label(config, &catalog.entries[index]);
      rib_host_load_bind(config, catalog.entries[index].id, catalog.entries[index].bind_index);

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

   focus_state.set(FocusRegion::Controls, FocusTarget::item(0));
   if (!load_file(defaults_path, true))
   {
      RARCH_ERR("[RIB] could not re-read controls from %s after changing "
            "controller; the menu still lists the previous pad.\n",
            defaults_path);
      return;
   }
   if (path[0])
      load_file(path, false);
   control_view.wire_controls(catalog);
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
   for (index = 0; index < catalog.count; ++index)
   {
      char key[96];

      if (!active(index))
         continue;
      snprintf(key, sizeof(key), "rib_label_%s", catalog.entries[index].id);
      config_set_string(config, key, catalog.entries[index].label);

      rib_host_write_bind(config, catalog.entries[index].id, catalog.entries[index].bind_index);
   }

   saved = rib_write_menu_config(config, path);
   config_file_free(config);
   return saved;
}

void Controls::refresh()
{
   const auto target = focus_state.target(FocusRegion::Controls);
   const int focused = target.kind == FocusTarget::Kind::Item ? target.index : -1;
   int index;
   for (index = 0; index < catalog.count; ++index)
   {
      char display_label[NAME_MAX_LENGTH * 2];
      char binding[4096];
      char group_id[96];

      if (!active(index))
         continue;
      strlcpy(display_label, catalog.entries[index].label,
            sizeof(display_label));
      callout_text(index, binding, sizeof(binding));
      control_view.set_control_state(catalog.entries[index].id,
            display_label, binding,
            screens.controls_visible() && focused == index,
            capture_active && capture_control == index);
      if (catalog.entries[index].group[0])
      {
         snprintf(group_id, sizeof(group_id), "%s%s", document_contract::ControlGroupBindingPrefix,
               catalog.entries[index].group);
         document.set_element_text(group_id, binding);
      }
   }
   control_view.set_controls_action_focus(
         screens.controls_visible() && target.kind == FocusTarget::Kind::Reset,
         screens.controls_visible() && target.kind == FocusTarget::Kind::Back,
         capture_active);
   if (screens.controls_visible() && focused >= 0
         && focused < catalog.count
         && catalog.entries[focused].group[0])
      control_view.focus_group(catalog.entries[focused].group);
   else
      control_view.focus_group(NULL);
}

void Controls::cancel_capture(const char *status)
{
   if (!capture_active)
      return;
   rib_host_capture_cancel();
   capture_active = false;
   this->status.set_controls(status ? status : rib::words::BindingUnchanged);
   screens.set_footer_hint(screens.controls_visible() ? rib::words::BackHint :
                                                       rib::words::ContinueHint);
   refresh();
}

void Controls::start_capture(int index)
{
   char status[96];
   if (!active(index))
      return;
   if (!rib_host_capture_start(catalog.entries[index].bind_index,
            RIB_CONTROL_CAPTURE_SECONDS))
   {
      this->status.set_controls(rib::words::CaptureFailed);
      return;
   }
   capture_active = true;
   capture_control = index;
   capture_ignore_pointer = true;
   snprintf(status, sizeof(status), rib::words::CaptureStarted,
         console_name(index));
   this->status.set_controls(status);
   screens.set_footer_hint(rib::words::CancelHint);
   refresh();
}

int Controls::find_conflict(int changed_index) const
{
   int index;
   if (!active(changed_index))
      return -1;
   for (index = 0; index < catalog.count; ++index)
   {
      if (index == changed_index || !active(index))
         continue;
      if (rib_host_bind_conflicts(catalog.entries[changed_index].bind_index,
               catalog.entries[index].bind_index))
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
         || left >= catalog.count || right >= catalog.count)
      return false;
   group_left = catalog.entries[left].group;
   group_right = catalog.entries[right].group;
   return group_left[0] && group_right[0] && string_is_equal(group_left, group_right);
}

void Controls::bind_anchor(int index, char *out, size_t length) const
{
   if (catalog.entries[index].group[0])
   {
      snprintf(out, length, "%s%s", document_contract::ControlGroupPrefix, catalog.entries[index].group);
      if (document.has_element(out))
         return;
   }
   snprintf(out, length, "%s%s", document_contract::ControlPrefix, catalog.entries[index].id);
   if (document.has_element(out))
      return;
   snprintf(out, length, "%s%s", document_contract::ControlHitPrefix, catalog.entries[index].id);
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
   if (index < 0 || index >= catalog.count)
   {
      strlcpy(out, rib::words::Unbound, length);
      return;
   }
   member_count = bind_members(index, members);

   for (slot = 0; slot < member_count; ++slot)
   {
      const unsigned at = catalog.entries[members[slot]].bind_index;
      rib_host_bind_lines(at,
            details, kinds, &lines);
   }
   if (lines <= 0)
   {
      strlcpy(out, rib::words::Unbound, length);
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
      const unsigned at = catalog.entries[members[member]].bind_index;
      rib_host_bind_lines(at,
            details, kinds, &lines);
      for (slot = before; slot < lines; ++slot)
      {
         const char *label = catalog.entries[members[member]].label;
         if (!label[0])
            label = catalog.entries[members[member]].id;
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

   rows = lists.rows_in(binds.list);
   if (lines > rows)
      RARCH_ERR("[RIB] '%s' has %d binds and the menu was built with %d rows; "
            "the rest are not shown.\n",
            catalog.entries[index].id, lines, rows);
   for (slot = 0; slot < rows; ++slot)
   {
      const char *id = lists.row_in(binds.list, slot);
      char row[64];
      if (!id || !*id)
         break;
      strlcpy(row, id, sizeof(row));
      if (slot < lines)
      {
         lists.set_row_text(row, titles[slot], details[slot], kinds[slot]);
         document.set_shown(row, true);
      }
      else
         document.set_shown(row, false);
   }
   lists.retarget_pages(binds.list);
   bind_anchor(index, anchor, sizeof(anchor));
   lists.place_list(binds.list, anchor, binds.width);
   binds.open = true;
}

void Controls::hide_binds()
{
   if (binds.list[0])
      document.set_shown(binds.list, false);
   binds.open = false;
}

void Controls::focus(FocusTarget target)
{
   if (target == focus_state.target(FocusRegion::Controls)) return;
   if (target.kind == FocusTarget::Kind::Item && !active(target.index)) return;
   if (target.kind == FocusTarget::Kind::Slots) return;
   focus_state.set(FocusRegion::Controls, target);
   refresh();
}

void Controls::update_binds(int x, int y, bool pointer_active, bool hover_active)
{
   int current = -1;
   rib::Event hovered;
   int64_t now;

   if (!binds.list[0] || !screens.controls_visible()
         || capture_active || device_picker_open)
   {
      hide_binds();
      binds.control = -1;
      return;
   }

   if (pointer_active
         && document.pointer_inside(binds.list, x, y)
         && binds.control >= 0)
      current = binds.control;
   else if (hover_active)
   {
      hovered = this->hovered;
      if (hovered.kind == RIB_RMLUI_ACTION_CONTROL)
      {
         const int index = index_of(hovered.id.c_str());
         if (active(index))
         {
            focus(FocusTarget::item(index));
            current = index;
         }
      }
      else if (hovered.kind == RIB_RMLUI_ACTION_CONTROLS_RESET
            || hovered.kind == RIB_RMLUI_ACTION_CONTROLS_BACK)
      {
         focus(hovered.kind == RIB_RMLUI_ACTION_CONTROLS_RESET
               ? FocusTarget::reset() : FocusTarget::back());
         current = -1;
      }
   }
   const auto target = focus_state.target(FocusRegion::Controls);
   const int focused = target.kind == FocusTarget::Kind::Item ? target.index : -1;
   if (current < 0 && focused >= 0
         && focused < catalog.count)
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

      for (index = 0; index < catalog.device_count; ++index)
         if (string_is_equal(catalog.devices[index].id, chosen))
         {
            device = catalog.devices[index].libretro;
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
               if (control_view.set_scene((const char*)markup))
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
   control_view.set_device_picker(catalog, false, profile_id);
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
      this->status.set_controls(rib::words::DefaultsLoadFailed);
   else if (!save())
      this->status.set_controls(rib::words::DefaultsSaveFailed);
   else
      this->status.set_controls(rib::words::DefaultsRestored);
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
                  rib::words::BindingConflict,
                  console_name(conflict));
            if (!save())
               strlcpy(capture_status, rib::words::BindingSaveFailed,
                     sizeof(capture_status));
            this->status.set_controls(capture_status);
         }
         else if (save())
            this->status.set_controls(rib::words::BindingSaved);
         else
            this->status.set_controls(rib::words::BindingSaveFailed);
         refresh();
         screens.set_footer_hint(rib::words::BackHint);
      }
      else if (result == RIB_CAPTURE_TIMED_OUT)
      {
         capture_active = false;
         this->status.set_controls(rib::words::CaptureTimeout);
         screens.set_footer_hint(rib::words::BackHint);
         refresh();
      }
      else
      {
         snprintf(capture_status, sizeof(capture_status),
               "%s: PRESS AN INPUT (%u)",
               console_name(capture_control),
               (unsigned)(remaining + 0.999f));
         this->status.set_controls(capture_status);
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
   control_view.set_device_picker(catalog, device_picker_open, profile_id);
}


int Controls::bind_members(int index, int *members) const
{
   if (!catalog.entries[index].group[0])
   {
      members[0] = index;
      return 1;
   }
   int found = 0;
   for (int cursor = 0; cursor < catalog.count; ++cursor)
      if (active(cursor) && string_is_equal(catalog.entries[cursor].group, catalog.entries[index].group))
         members[found++] = cursor;
   return found;
}

}
