#include "declarations.h"

#include "../../../verbosity.h"
#include <file/config_file.h>
#include <file/file_path.h>
#include <retro_miscellaneous.h>
#include <string/stdstring.h>
#include <cstdio>
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
