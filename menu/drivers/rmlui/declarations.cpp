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

/* A value as it is in design.cfg, whole. */
bool read(config_file_t *config, const std::string& key, std::string& out)
{
   const struct config_entry_list *entry = config_get_entry(config, key.c_str());
   if (!entry || !entry->value)
      return false;
   out = entry->value;
   return true;
}

std::string value(config_file_t *config, const std::string& key)
{
   std::string out;
   read(config, key, out);
   return out;
}

int number(config_file_t *config, const std::string& key, int otherwise)
{
   int out = otherwise;
   return config_get_int(config, key.c_str(), &out) ? out : otherwise;
}

/* The ids in a space-separated list, in order. */
std::vector<std::string> ids(const std::string& list)
{
   std::vector<std::string> found;
   size_t at = 0;
   while (at < list.size())
   {
      const size_t start = list.find_first_not_of(' ', at);
      if (start == std::string::npos)
         break;
      const size_t end = list.find(' ', start);
      found.push_back(list.substr(start, end == std::string::npos ? std::string::npos : end - start));
      at = end == std::string::npos ? list.size() : end;
   }
   return found;
}

void screens(config_file_t *config, rib::DesignDeclarations& design)
{
   for (const std::string& id : ids(value(config, "screens")))
   {
      rib::ScreenDeclaration screen;
      screen.id = id;
      if (!read(config, "screen_panel_" + id, screen.panel) || screen.panel.empty())
         continue;
      screen.heading = value(config, "screen_heading_" + id);
      screen.footer = value(config, "screen_footer_" + id);
      screen.buttons = ids(value(config, "screen_button_" + id));
      screen.images = value(config, "screen_images_" + id);
      std::string role;
      if (read(config, "screen_role_" + id, role))
      {
         screen.role = rib::screen_role(role);
         if (screen.role == rib::ScreenRole::None)
            RARCH_ERR("[RIB] the screen '%s' takes the role '%s', which this player "
                  "does not know; it is an ordinary screen.\n", id.c_str(), role.c_str());
      }
      design.screens.push_back(std::move(screen));
   }
}

/* The key and the kind in design.cfg, by the words in settings.inc. */
bool setting_key_named(const std::string& word, rib_setting_key& key)
{
#define RIB_SETTING_KEY(name, retroarch) \
   if (word == retroarch) { key = RIB_SETTING_##name; return true; }
#include "settings.inc"
   return false;
}

bool setting_kind_named(const std::string& word, rib::SettingKind& kind)
{
#define RIB_SETTING_KIND(name, text) \
   if (word == text) { kind = rib::SettingKind::name; return true; }
#include "settings.inc"
   return false;
}

void settings(config_file_t *config, rib::DesignDeclarations& design)
{
   for (const std::string& id : ids(value(config, "settings")))
   {
      rib::SettingDeclaration setting;
      setting.id = id;
      setting.control = value(config, "setting_control_" + id);
      setting.file = value(config, "setting_file_" + id);
      const std::string kind = value(config, "setting_kind_" + id);
      const std::string key = value(config, "setting_key_" + id);
      if (!setting_kind_named(kind, setting.kind))
      {
         RARCH_ERR("[RIB] the setting '%s' is a '%s', which this player does "
               "not implement; it will not be shown.\n", id.c_str(), kind.c_str());
         continue;
      }
      if (!setting_key_named(key, setting.key))
      {
         RARCH_ERR("[RIB] the setting '%s' drives '%s', which this player does "
               "not apply; it will not be shown.\n", id.c_str(), key.c_str());
         continue;
      }
      switch (setting.kind)
      {
         case rib::SettingKind::Level:
            setting.low = strtof(value(config, "setting_low_" + id).c_str(), NULL);
            setting.high = strtof(value(config, "setting_high_" + id).c_str(), NULL);
            setting.positions = number(config, "setting_positions_" + id, 0);
            break;
         case rib::SettingKind::Switch:
            setting.inverted = value(config, "setting_inverted_" + id) == "true";
            break;
      }
      if (setting.control.empty() || setting.file.empty()
            || (setting.kind == rib::SettingKind::Level
               && (setting.positions < 2 || setting.high == setting.low)))
      {
         RARCH_ERR("[RIB] the setting '%s' is declared without its control, "
               "file or range; it will not be shown.\n", id.c_str());
         continue;
      }
      design.settings.push_back(std::move(setting));
   }
}

void overlays(config_file_t *config, const char *assets, rib::DesignDeclarations& design)
{
   for (const std::string& id : ids(value(config, "overlays")))
   {
      rib::OverlayDeclaration overlay;
      overlay.id = id;
      overlay.after_ms = number(config, "overlay_after_" + id, 0);
      overlay.hold_ms = number(config, "overlay_hold_" + id, 0);
      if (overlay.hold_ms <= 0)
         continue;
      overlay.leave_ms = number(config, "overlay_leave_" + id, 0);
      overlay.follows = value(config, "overlay_follows_" + id);
      overlay.needs = value(config, "overlay_needs_" + id);
      if (!overlay.needs.empty()
            && !path_is_valid((std::string(assets) + "/" + overlay.needs).c_str()))
      {
         RARCH_LOG("[RIB] overlay '%s' needs %s, which this game does not "
               "carry; it will not be drawn.\n", id.c_str(), overlay.needs.c_str());
         continue;
      }
      design.overlays.push_back(std::move(overlay));
   }
}
}

namespace rib {
const char *setting_key_name(rib_setting_key key)
{
   switch (key)
   {
#define RIB_SETTING_KEY(name, retroarch) case RIB_SETTING_##name: return retroarch;
#include "settings.inc"
      default:
         return "";
   }
}

DesignDeclarations load_design(const char *asset_directory)
{
   DesignDeclarations design;
   if (!asset_directory || !*asset_directory)
      return design;
   const std::string path = std::string(asset_directory) + "/design.cfg";
   std::unique_ptr<config_file_t, decltype(&config_file_free)> config(
         config_file_new_from_path_to_string(path.c_str()), config_file_free);
   if (!config)
   {
      RARCH_LOG("[RIB] no design declarations at %s; the menu has no screens "
            "and nothing will switch.\n", path.c_str());
      return design;
   }
   screens(config.get(), design);
   settings(config.get(), design);
   overlays(config.get(), asset_directory, design);
   design.fonts = ids(value(config.get(), "fonts"));
   design.binds.list = value(config.get(), "binds_list");
   design.binds.after_ms = number(config.get(), "binds_after", design.binds.after_ms);
   design.binds.hover_after_ms = number(config.get(), "binds_hover_after", design.binds.hover_after_ms);
   design.binds.width = number(config.get(), "binds_width", design.binds.width);
   for (size_t index = 0; index < kWordCount; ++index)
   {
      const Word word = static_cast<Word>(index);
      std::string text;
      if (read(config.get(), std::string("word_") + word_id(word), text))
         design.words.emplace_back(word, text);
   }
   return design;
}
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
