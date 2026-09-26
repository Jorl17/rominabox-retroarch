#include "declarations.h"

#include "../../../verbosity.h"
#include <file/config_file.h>
#include <file/file_path.h>
#include <retro_miscellaneous.h>
#include <string/stdstring.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <memory>
#include <vector>

namespace
{
/* A value as it is in its file, whole. */
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

void discover_controls(config_file_t *config, const std::string& profile_id,
      rib_controls_catalog *catalog,
      bool (*bind_index_resolver)(const char *, unsigned *))
{
   struct config_file_entry entry;
   bool present;
   /* The controls of a pad, when the defaults list them, or else every control. */
   std::vector<std::string> belonging;
   if (!profile_id.empty())
      belonging = ids(value(config, "controls_variant_controls_" + profile_id));

   catalog->count = 0;
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
      if (!belonging.empty()
            && std::find(belonging.begin(), belonging.end(), id) == belonging.end())
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
      rib_control_declaration& control = catalog->entries[catalog->count];
      control.id = id;
      control.group = value(config, std::string("rib_group_") + id);
      control.bind_index = bind_index;
      ++catalog->count;
   }
}

void discover_devices(config_file_t *config, rib_controls_catalog *catalog)
{
   catalog->device_count = 0;
   for (const std::string& id : ids(value(config, "controls_variants")))
   {
      if (catalog->device_count >= RIB_DEVICE_MAX)
      {
         RARCH_ERR("[RIB] more than %d controllers offered; '%s' and any after "
               "it cannot be chosen.\n", RIB_DEVICE_MAX, id.c_str());
         return;
      }
      rib_device_declaration& device = catalog->devices[catalog->device_count];
      device.id = id;
      device.libretro = (unsigned)strtoul(
            value(config, "controls_variant_device_" + id).c_str(), NULL, 10);
      if (!read(config, "controls_variant_name_" + id, device.name))
         device.name = id;
      ++catalog->device_count;
   }
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

/* A level's values, one per position, from a space-separated list. None
 * when any of them is not a number. */
std::vector<float> levels(const std::string& list)
{
   std::vector<float> values;
   for (const std::string& word : ids(list))
   {
      char *end = NULL;
      values.push_back(strtof(word.c_str(), &end));
      if (end == word.c_str() || *end)
         return {};
   }
   return values;
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
            setting.values = levels(value(config, "setting_values_" + id));
            break;
         case rib::SettingKind::Switch:
            setting.inverted = value(config, "setting_inverted_" + id) == "true";
            break;
      }
      if (setting.control.empty() || setting.file.empty()
            || (setting.kind == rib::SettingKind::Level && setting.values.size() < 2))
      {
         RARCH_ERR("[RIB] the setting '%s' is declared without its control, "
               "file or values; it will not be shown.\n", id.c_str());
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

   const std::string path = std::string(asset_directory) + "/shaders.cfg";
   std::unique_ptr<config_file_t, decltype(&config_file_free)> config(
         config_file_new_from_path_to_string(path.c_str()), config_file_free);
   if (!config)
      return;
   for (const std::string& id : ids(value(config.get(), "shader_ids")))
   {
      if (catalog->count >= RIB_SHADER_MAX)
      {
         RARCH_ERR("[RIB] shader list has more than %d entries; '%s' and the "
               "rest are not offered.\n", RIB_SHADER_MAX, id.c_str());
         break;
      }
      rib_shader_declaration& shader = catalog->entries[catalog->count++];
      shader.id = id;
      shader.preset = value(config.get(), "shader_preset_" + id);
   }
}

config_file_t *rib_open_controls(const char *path, bool defaults,
      std::string& profile_id, rib_controls_catalog *catalog,
      bool *profile_present, bool (*bind_index)(const char *, unsigned *))
{
   if (!path || !catalog || !profile_present || !bind_index)
      return nullptr;
   config_file_t *config = config_file_new_from_path_to_string(path);
   if (!config)
      return nullptr;

   const std::string profile = value(config, "controls_profile");
   *profile_present = !profile.empty();
   if (*profile_present)
      profile_id = profile;
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
      offered = offered || catalog->devices[index].id == profile;
   if (offered)
      discover_controls(defaults, profile, catalog, bind_index);
   return offered;
}

void rib_controls_read_enabled(config_file_t *config,
      rib_controls_catalog *catalog)
{
   for (int index = 0; index < catalog->count; ++index)
   {
      rib_control_declaration& control = catalog->entries[index];
      control.enabled = false;
      for (const char *suffix : {"", "_btn", "_axis", "_mbtn"})
         if (config_get_entry(config, ("input_player1_" + control.id + suffix).c_str()))
         {
            control.enabled = true;
            break;
         }
   }
}

void rib_controls_read_label(config_file_t *config,
      rib_control_declaration *control)
{
   read(config, "rib_label_" + control->id, control->label);
}
