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
#include <string>

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

namespace {
/* The element for a stop on the pad screen. The controls of a stick share the
 * box of their group, and any other control is its callout. */
std::string stop_id(const rib_controls_catalog& catalog, FocusTarget target)
{
   switch (target.kind)
   {
      case FocusTarget::Kind::Reset: return document_contract::ControlsReset;
      case FocusTarget::Kind::Back: return document_contract::ControlsBack;
      case FocusTarget::Kind::Item: break;
   }
   const auto& entry = catalog.entries[target.index];
   if (entry.group[0])
      return document_contract::ControlGroupPrefix + std::string(entry.group);
   return document_contract::ControlPrefix + std::string(entry.id);
}

/* The focused stop in RmlUi, as a stop of the pad screen, with an item index
 * of -1 when it is none of them. For a stick, its first member. */
FocusTarget focused_stop(const rib_controls_catalog& catalog, const std::string& id)
{
   if (id == document_contract::ControlsReset) return FocusTarget::reset();
   if (id == document_contract::ControlsBack) return FocusTarget::back();
   for (int index = 0; index < catalog.count; ++index)
      if (catalog.entries[index].enabled && stop_id(catalog, FocusTarget::item(index)) == id)
         return FocusTarget::item(index);
   return FocusTarget::item(-1);
}
}

/* At startup we read the author's defaults, then the player's file. When the
 * player chose another pad, we use that pad. Otherwise we only put the
 * player's labels and bindings over the author's. */
bool Controls::load_file(const char *file, bool defaults)
{
   if (defaults)
   {
      strlcpy(defaults_path, file, sizeof(defaults_path));
      return apply(NULL, false);
   }
   if (file != path)
      strlcpy(path, file, sizeof(path));
   const std::string chosen = player_profile();
   if (!chosen.empty() && chosen != profile_id)
      return apply(chosen.c_str(), true);
   const bool read = read_player_file();
   show_pad();
   return read;
}

/* The one way we apply a pad: its controls, labels and bindings from the
 * author's defaults, then the player's file unless the player pressed Reset,
 * and then the picture and the name in the picker. `wanted` is a pad the
 * player chose. With NULL we keep the pad in the player's file, or else the
 * author's. */
bool Controls::apply(const char *wanted, bool player_file)
{
   const std::string chosen = !wanted && player_file ? player_profile() : std::string();
   if (!chosen.empty())
      wanted = chosen.c_str();
   if (!read_defaults(wanted))
      return false;
   if (player_file)
      read_player_file();
   show_pad();
   return true;
}

bool Controls::read_defaults(const char *wanted)
{
   config_file_t *config;
   bool present = false;
   int index;

   if (!defaults_path[0])
   {
      const char *assets = getenv("ROMINABOX_RML_ASSETS");
      snprintf(defaults_path, sizeof(defaults_path), "%s/controls-defaults.cfg",
            assets && *assets ? assets : RIB_RMLUI_DEFAULT_ASSETS);
   }
   /* We find the author's pad when we open the defaults. For a pad that this
    * game does not offer, we use the author's pad instead. */
   exported_profile[0] = '\0';
   if (!(config = rib_open_controls(defaults_path, true, exported_profile,
               &catalog, &present, rib_host_bind_index)))
      return false;
   if (!wanted || !*wanted
         || !rib_controls_discover(config, wanted, &catalog, rib_host_bind_index))
      wanted = exported_profile;
   strlcpy(profile_id, wanted, sizeof(profile_id));
   rib_controls_read_enabled(config, &catalog);
   for (index = 0; index < catalog.count; ++index)
   {
      if (!active(index))
         continue;
      catalog.entries[index].label[0] = '\0';
      rib_host_clear_bind(catalog.entries[index].bind_index);
      rib_controls_read_label(config, &catalog.entries[index]);
      rib_host_load_bind(config, catalog.entries[index].id, catalog.entries[index].bind_index);
   }
   rib_host_restore_keyboard_mapping();
   config_file_free(config);
   return true;
}

/* The pad in the player's file, empty when there is none. */
std::string Controls::player_profile()
{
   char named[32] = "";
   bool present = false;
   if (path[0])
      if (config_file_t *config = rib_open_controls(path, false, named, &catalog,
               &present, rib_host_bind_index))
         config_file_free(config);
   return named;
}

bool Controls::read_player_file()
{
   char named[32] = "";
   bool present = false;
   config_file_t *config;
   int index;

   if (!path[0] || !(config = rib_open_controls(path, false, named, &catalog,
               &present, rib_host_bind_index)))
      return false;
   for (index = 0; index < catalog.count; ++index)
   {
      if (!active(index))
         continue;
      rib_controls_read_label(config, &catalog.entries[index]);
      rib_host_load_bind(config, catalog.entries[index].id, catalog.entries[index].bind_index);
   }
   rib_host_restore_keyboard_mapping();
   config_file_free(config);
   return true;
}

/* Draw the pad we apply and show its name in the picker. The exported
 * document already contains the author's pad, and we read any other pad from
 * its scene in the export. */
void Controls::show_pad()
{
   const char *drawn = control_view.scene_profile();
   if (!*drawn)
      drawn = exported_profile;
   if (!string_is_equal(drawn, profile_id))
   {
      const char *assets = getenv("ROMINABOX_RML_ASSETS");
      char scene[PATH_MAX_LENGTH];
      int64_t length = 0;
      void *markup = NULL;
      snprintf(scene, sizeof(scene), "%s/scene-%s.rml",
            assets && *assets ? assets : RIB_RMLUI_DEFAULT_ASSETS, profile_id);
      if (filestream_read_file(scene, &markup, &length) && markup
            && control_view.set_scene(profile_id, (const char*)markup))
         RARCH_LOG("[RIB] drawing '%s' from %s.\n", profile_id, scene);
      else
         RARCH_ERR("[RIB] no scene for '%s' at %s; the pad on screen "
               "is still the one the game was exported with.\n", profile_id, scene);
      free(markup);
   }
   control_view.set_device_picker(catalog, device_picker_open, profile_id);
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
            capture_active && capture_control == index);
      if (catalog.entries[index].group[0])
      {
         snprintf(group_id, sizeof(group_id), "%s%s", document_contract::ControlGroupBindingPrefix,
               catalog.entries[index].group);
         document.set_element_text(group_id, binding);
      }
   }
   control_view.set_capturing(capture_active);
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
   if (target.kind == FocusTarget::Kind::Item && !active(target.index)) return;
   focus_state.set(stop_id(catalog, target).c_str());
}

void Controls::update_binds(int x, int y, bool pointer_active)
{
   int current = -1;
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
   const auto target = focused_stop(catalog, focus_state.current_id());
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
   /* We open the list sooner for a pointer resting on the control than for a
    * key press that only passed through it on the way elsewhere. */
   const int after_ms = this->hovered.kind == RIB_RMLUI_ACTION_CONTROL
         && index_of(this->hovered.id.c_str()) == current
         ? binds.hover_after_ms : binds.after_ms;
   now = rib_host_time_us();
   if (now - binds.since >= (int64_t)after_ms * 1000)
      show_binds(current);
}

void Controls::choose_device(const char *chosen)
{
   device_picker_open = false;
   if (chosen && *chosen && !string_is_equal(chosen, profile_id))
   {
      int index;

      strlcpy(profile_id, chosen, sizeof(profile_id));
      /* The pad belongs to the player who picks it, so we write it to the
       * per-game override and never to the author's fixed defaults. */
      save();

      /* We apply it to the core now, not at the next launch. The
       * emulated device is a setting in RetroArch, and we apply it again
       * with CMD_EVENT_CONTROLLER_INIT, as for any other change of device
       * in the frontend. A catalog device of 0 means "no subclass". The
       * default in the frontend is then a joypad, and writing 0 would
       * connect nothing. At the next launch we read the device from the
       * remap and not from the per-game override. */
      for (index = 0; index < catalog.device_count; ++index)
         if (string_is_equal(catalog.devices[index].id, chosen))
         {
            rib_host_apply_device(chosen, catalog.devices[index].libretro);
            break;
         }

      if (!apply(chosen, true))
         RARCH_ERR("[RIB] could not re-read controls from %s after changing "
               "controller; the menu still lists the previous pad.\n",
               defaults_path);
      focus(first());
      refresh();
   }
   control_view.set_device_picker(catalog, false, profile_id);
}

void Controls::reset_defaults()
{
   if (capture_active)
      cancel_capture(NULL);
   device_picker_open = false;
   if (!apply(NULL, false))
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
   binds.hover_after_ms = design.binds_hover_after_ms;
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
