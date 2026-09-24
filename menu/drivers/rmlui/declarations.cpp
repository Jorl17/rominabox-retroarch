#include "declarations.h"

#include "../../../verbosity.h"
#include <file/config_file.h>
#include <file/file_path.h>
#include <retro_miscellaneous.h>
#include <string/stdstring.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>

struct rib_design_declarations
{
   rib_design_data data{};
   std::vector<rib_screen_declaration> screens;
};

namespace
{
/* The sizes of the list buffers are limits on the input we accept here. */
template<size_t Capacity, typename Visit>
void each_id(config_file_t *config, const char *key, Visit visit)
{
   char list[Capacity];
   if (!config_get_array(config, key, list, sizeof(list)))
      return;
   char *cursor = list;
   while (char *id = strtok_r(cursor, " ", &cursor))
      if (*id && !visit(id))
         break;
}

bool control_belongs(const char *list, const char *id)
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

void discover_controls(config_file_t *config, const char *profile_id,
      rib_controls_catalog *catalog,
      bool (*bind_index_resolver)(const char *, unsigned *))
{
   struct config_file_entry entry;
   bool present;
   char key[96];
   char belonging[1024];

   catalog->count = 0;
   belonging[0] = '\0';
   if (profile_id[0])
   {
      snprintf(key, sizeof(key), "controls_variant_controls_%s", profile_id);
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
      if (!bind_index_resolver(id, &bind_index))
      {
         RARCH_WARN("[RIB] '%s' is not a libretro bind; the menu will not show "
               "it. Check the id against DECLARE_BIND in configuration.c.\n", id);
         continue;
      }
      if (catalog->count >= RIB_CONTROL_MAX)
      {
         /* We log this, so that a control left out, such as a DualShock
          * stick, appears in the log. */
         RARCH_ERR("[RIB] more than %d controls declared; '%s' and anything "
               "after it are unreachable.\n", RIB_CONTROL_MAX, id);
         return;
      }
      strlcpy(catalog->entries[catalog->count].id, id,
            sizeof(catalog->entries[catalog->count].id));
      catalog->entries[catalog->count].group[0] = '\0';
      {
         char group_key[96];
         snprintf(group_key, sizeof(group_key), "rib_group_%s", id);
         config_get_array(config, group_key,
               catalog->entries[catalog->count].group,
               sizeof(catalog->entries[catalog->count].group));
      }
      catalog->entries[catalog->count].bind_index = bind_index;
      ++catalog->count;
   }
}

void discover_devices(config_file_t *config, rib_controls_catalog *catalog)
{
   catalog->device_count = 0;
   each_id<512>(config, "controls_variants", [&](const char *token) {
      char key[96];
      char name[NAME_MAX_LENGTH];

      if (catalog->device_count >= RIB_DEVICE_MAX)
      {
         RARCH_ERR("[RIB] more than %d controllers offered; '%s' and any after "
               "it cannot be chosen.\n", RIB_DEVICE_MAX, token);
         return false;
      }
      strlcpy(catalog->devices[catalog->device_count].id, token,
            sizeof(catalog->devices[catalog->device_count].id));
      snprintf(key, sizeof(key), "controls_variant_device_%s", token);
      catalog->devices[catalog->device_count].libretro = 0;
      {
         char device[32];
         if (config_get_array(config, key, device, sizeof(device)))
            catalog->devices[catalog->device_count].libretro =
               (unsigned)strtoul(device, NULL, 10);
      }
      snprintf(key, sizeof(key), "controls_variant_name_%s", token);
      if (config_get_array(config, key, name, sizeof(name)))
         strlcpy(catalog->devices[catalog->device_count].name, name,
               sizeof(catalog->devices[catalog->device_count].name));
      else
         strlcpy(catalog->devices[catalog->device_count].name, token,
               sizeof(catalog->devices[catalog->device_count].name));
      ++catalog->device_count;
      return true;
   });
}

void screens(config_file_t *config, rib_design_declarations &design)
{
   each_id<512>(config, "screens", [&](const char *id) {
      char key[96];
      rib_screen_declaration screen{};
      strlcpy(screen.id, id, sizeof(screen.id));
      snprintf(key, sizeof(key), "screen_panel_%s", id);
      if (!config_get_array(config, key, screen.panel, sizeof(screen.panel)))
         return true;
      snprintf(key, sizeof(key), "screen_heading_%s", id);
      if (!config_get_array(config, key, screen.heading, sizeof(screen.heading)))
         screen.heading[0] = '\0';
      snprintf(key, sizeof(key), "screen_footer_%s", id);
      if (!config_get_array(config, key, screen.footer, sizeof(screen.footer)))
         screen.footer[0] = '\0';
      snprintf(key, sizeof(key), "screen_button_%s", id);
      if (!config_get_array(config, key, screen.button, sizeof(screen.button)))
         screen.button[0] = '\0';
      snprintf(key, sizeof(key), "screen_images_%s", id);
      if (!config_get_array(config, key, screen.images, sizeof(screen.images)))
         screen.images[0] = '\0';
      snprintf(key, sizeof(key), "screen_mark_%s", id);
      if (!config_get_array(config, key, screen.mark, sizeof(screen.mark)))
         screen.mark[0] = '\0';
      design.screens.push_back(screen);
      return true;
   });
   design.data.screens = design.screens.data();
   design.data.screen_count = design.screens.size();
}

void toggles(config_file_t *config, rib_design_data &data)
{
   each_id<256>(config, "toggles", [&](const char *id) {
      if (data.toggle_count >= RIB_TOGGLE_MAX)
      {
         RARCH_ERR("[RIB] more than %d switches are declared; '%s' and any "
               "after it will not work.\n", RIB_TOGGLE_MAX, id);
         return false;
      }
      char key[128];
      char value[128] = {};
      rib_toggle_t &toggle = data.toggles[data.toggle_count++];
      strlcpy(toggle.id, id, sizeof(toggle.id));
      snprintf(key, sizeof(key), "toggle_on_%s", id);
      config_get_array(config, key, toggle.on, sizeof(toggle.on));
      snprintf(key, sizeof(key), "toggle_off_%s", id);
      config_get_array(config, key, toggle.off, sizeof(toggle.off));
      snprintf(key, sizeof(key), "toggle_default_%s", id);
      config_get_array(config, key, value, sizeof(value));
      toggle.state = string_is_equal(value, "true");
      snprintf(key, sizeof(key), "toggle_guard_%s", id);
      value[0] = '\0';
      config_get_array(config, key, value, sizeof(value));
      if (!*value)
         toggle.guard = RIB_TOGGLE_GUARD_NONE;
      else if (string_is_equal(value, "saves"))
         toggle.guard = RIB_TOGGLE_GUARD_SAVES;
      else
      {
         RARCH_ERR("[RIB] the switch '%s' guards '%s', which this player does "
               "not implement; it will guard nothing.\n", id, value);
         toggle.guard = RIB_TOGGLE_GUARD_NONE;
      }
      snprintf(key, sizeof(key), "toggle_guard_label_%s", id);
      config_get_array(config, key, toggle.guard_label, sizeof(toggle.guard_label));
      snprintf(key, sizeof(key), "toggle_guard_status_%s", id);
      config_get_array(config, key, toggle.guard_status, sizeof(toggle.guard_status));
      return true;
   });
}

void overlays(config_file_t *config, const char *assets, rib_design_data &data)
{
   each_id<512>(config, "overlays", [&](const char *id) {
      if (data.overlay_count >= RIB_OVERLAY_MAX)
      {
         RARCH_WARN("[RIB] the design declares more than %d overlays; '%s' and "
               "anything after it will not be drawn.\n", RIB_OVERLAY_MAX, id);
         return false;
      }
      char key[96];
      rib_overlay_declaration overlay{};
      snprintf(key, sizeof(key), "overlay_after_%s", id);
      config_get_int(config, key, &overlay.after_ms);
      snprintf(key, sizeof(key), "overlay_hold_%s", id);
      if (!config_get_int(config, key, &overlay.hold_ms) || overlay.hold_ms <= 0)
         return true;
      snprintf(key, sizeof(key), "overlay_leave_%s", id);
      config_get_int(config, key, &overlay.leave_ms);
      snprintf(key, sizeof(key), "overlay_follows_%s", id);
      if (!config_get_array(config, key, overlay.follows, sizeof(overlay.follows)))
         overlay.follows[0] = '\0';
      snprintf(key, sizeof(key), "overlay_needs_%s", id);
      if (!config_get_array(config, key, overlay.needs, sizeof(overlay.needs)))
         overlay.needs[0] = '\0';
      if (*overlay.needs)
      {
         char required[PATH_MAX_LENGTH];
         snprintf(required, sizeof(required), "%s/%s", assets, overlay.needs);
         if (!path_is_valid(required))
         {
            RARCH_LOG("[RIB] overlay '%s' needs %s, which this game does not "
                  "carry; it will not be drawn.\n", id, overlay.needs);
            return true;
         }
      }
      strlcpy(overlay.id, id, sizeof(overlay.id));
      data.overlays[data.overlay_count++] = overlay;
      return true;
   });
}
}

rib_design_declarations *rib_load_design(const char *asset_directory)
{
   auto design = std::make_unique<rib_design_declarations>();
   if (!asset_directory || !*asset_directory)
      return design.release();
   char path[PATH_MAX_LENGTH];
   snprintf(path, sizeof(path), "%s/design.cfg", asset_directory);
   std::unique_ptr<config_file_t, decltype(&config_file_free)> config(
         config_file_new_from_path_to_string(path), config_file_free);
   if (!config)
   {
      RARCH_LOG("[RIB] no design declarations at %s; the menu has no screens "
            "and nothing will switch.\n", path);
      return design.release();
   }
   screens(config.get(), *design);
   toggles(config.get(), design->data);
   overlays(config.get(), asset_directory, design->data);
   if (!config_get_array(config.get(), "binds_list", design->data.binds_list,
         sizeof(design->data.binds_list)))
      design->data.binds_list[0] = '\0';
   config_get_int(config.get(), "binds_after", &design->data.binds_after_ms);
   /* When the export does not contain it, the pointer rest is 300 ms. */
   design->data.binds_hover_after_ms = 300;
   config_get_int(config.get(), "binds_hover_after", &design->data.binds_hover_after_ms);
   config_get_int(config.get(), "binds_width", &design->data.binds_width);
   return design.release();
}

const rib_design_data *rib_design_get(const rib_design_declarations *design)
{
   return &design->data;
}

void rib_design_free(rib_design_declarations *design)
{
   delete design;
}

void rib_load_shaders(const char *asset_directory, rib_shader_catalog *catalog)
{
   if (!catalog)
      return;
   *catalog = {};
   if (!asset_directory || !*asset_directory)
      return;

   char path[PATH_MAX_LENGTH];
   snprintf(path, sizeof(path), "%s/shaders.cfg", asset_directory);
   config_file_t *config = config_file_new_from_path_to_string(path);
   if (!config)
      return;
   config_get_array(config, "shader_state_on", catalog->state_on,
         sizeof(catalog->state_on));
   config_get_array(config, "shader_state_off", catalog->state_off,
         sizeof(catalog->state_off));
   each_id<1024>(config, "shader_ids", [&](const char *id) {
      if (catalog->count >= RIB_SHADER_MAX)
      {
         RARCH_ERR("[RIB] shader list has more than %d entries; the rest "
               "are not offered.\n", RIB_SHADER_MAX);
         return false;
      }
      char key[96];
      char preset[PATH_MAX_LENGTH];
      rib_shader_declaration &shader = catalog->entries[catalog->count];
      strlcpy(shader.id, id, sizeof(shader.id));
      snprintf(key, sizeof(key), "shader_preset_%s", id);
      preset[0] = '\0';
      config_get_array(config, key, preset, sizeof(preset));
      strlcpy(shader.preset, preset, sizeof(shader.preset));
      ++catalog->count;
      return true;
   });
   config_file_free(config);
}

config_file_t *rib_open_controls(const char *path, bool defaults,
      char profile_id[32], rib_controls_catalog *catalog,
      bool *profile_present, bool (*bind_index)(const char *, unsigned *))
{
   if (!path || !profile_id || !catalog || !profile_present || !bind_index)
      return nullptr;
   config_file_t *config = config_file_new_from_path_to_string(path);
   if (!config)
      return nullptr;

   char profile[32] = {0};
   const bool complete = config_get_array(config, "controls_profile", profile,
         sizeof(profile));
   // For an oversized override, we still repaint the picker with the old id.
   *profile_present = profile[0] != '\0';
   if (complete && *profile_present)
      strlcpy(profile_id, profile, 32);
   if (defaults)
   {
      discover_controls(config, profile_id, catalog, bind_index);
      discover_devices(config, catalog);
   }
   return config;
}

bool rib_controls_discover(config_file_t *defaults, const char *profile,
      rib_controls_catalog *catalog, bool (*bind_index)(const char *, unsigned *))
{
   if (!defaults || !profile || !catalog || !bind_index)
      return false;
   bool offered = false;
   for (int index = 0; index < catalog->device_count; ++index)
      offered = offered || string_is_equal(catalog->devices[index].id, profile);
   if (offered)
      discover_controls(defaults, profile, catalog, bind_index);
   return offered;
}

void rib_controls_read_enabled(config_file_t *config,
      rib_controls_catalog *catalog)
{
   for (int index = 0; index < catalog->count; ++index)
   {
      char key[96];
      const char *suffixes[] = {"", "_btn", "_axis", "_mbtn"};
      unsigned suffix_index;
      catalog->entries[index].enabled = false;
      for (suffix_index = 0; suffix_index < ARRAY_SIZE(suffixes);
            ++suffix_index)
      {
         snprintf(key, sizeof(key), "input_player1_%s%s",
               catalog->entries[index].id, suffixes[suffix_index]);
         if (config_get_entry(config, key))
         {
            catalog->entries[index].enabled = true;
            break;
         }
      }
   }
}

void rib_controls_read_label(config_file_t *config,
      rib_control_declaration *control)
{
   char key[64];
   char label[NAME_MAX_LENGTH] = {0};
   snprintf(key, sizeof(key), "rib_label_%s", control->id);
   if (config_get_array(config, key, label, sizeof(label)))
      strlcpy(control->label, label, sizeof(control->label));
}
