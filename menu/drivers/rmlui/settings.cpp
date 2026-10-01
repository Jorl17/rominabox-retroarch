#include "settings.hpp"
#include "setting_display.hpp"
#include "host.h"
#include "files.h"
#include "document.hpp"
#include "parts.hpp"
#include "slots.hpp"
#include "sounds.hpp"
#include "../../../verbosity.h"
#include <streams/file_stream.h>
#include <string/stdstring.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace rib {
namespace {
/* A value as we write it in the file of the setting, for RetroArch to read
 * back: a level to one decimal, and a switch as true or false. */
std::string file_text(const SettingDeclaration& setting, float value)
{
   char text[32];
   if (setting.kind == SettingKind::Level)
      snprintf(text, sizeof(text), "%.1f", value);
   else
      strlcpy(text, value != 0.0f ? "true" : "false", sizeof(text));
   return text;
}
}

void PlayerSettings::load(const DesignDeclarations& design, const char *data_directory)
{
   entries = design.settings;
   data = data_directory ? data_directory : "";
}

const SettingDeclaration *PlayerSettings::owning(const char *control,
      SettingKind kind) const
{
   if (!control || !*control)
      return nullptr;
   for (const SettingDeclaration& setting : entries)
      if (setting.kind == kind && setting.control == control)
         return &setting;
   return nullptr;
}

float PlayerSettings::value(const SettingDeclaration& setting) const
{
   float current = setting.default_value;
   rib_host_setting(setting.key, &current);
   return current;
}

void PlayerSettings::attach()
{
   for (const SettingDeclaration& setting : entries)
   {
      if (setting.kind != SettingKind::Level)
         continue;
      parts.set_slider_step(setting.control.c_str(), level_fraction_at(setting, 1));
      /* We move a level between positions, from a file or a hotkey, to the
       * nearest one and store it there. The file has one decimal, so a level
       * from it is at a position when it matches to one decimal, and we do not
       * write a level that is already at a position. */
      const float current = value(setting);
      const float nearest = setting.values[level_position_of(setting, current)];
      if (file_text(setting, nearest) != file_text(setting, current))
         set(setting, nearest, true);
   }
   paint();
}

void PlayerSettings::paint() const
{
   for (const SettingDeclaration& setting : entries)
      paint_setting(document.root(), setting, value(setting),
            rib_host_setting_used(setting.key), parts);
}

void PlayerSettings::set(const SettingDeclaration& setting, float chosen, bool persist)
{
   if (!rib_host_set_setting(setting.key, chosen))
   {
      RARCH_ERR("[RIB] the setting '%s' drives '%s', which this player cannot "
            "change while the game runs.\n", setting.id.c_str(),
            setting_key_name(setting.key));
      return;
   }
   if (!persist || data.empty())
      return;
   const std::string text = file_text(setting, chosen);
   const std::string path = data + "/" + setting.file;
   if (!rib_write_player_setting(path.c_str(), setting_key_name(setting.key), text.c_str()))
      RARCH_ERR("[RIB] '%s' is %s now, but %s could not be written, so the "
            "next launch will start from the export's default.\n",
            setting.id.c_str(), text.c_str(), path.c_str());
}

bool PlayerSettings::slide(const char *control, float fraction, bool persist)
{
   const SettingDeclaration *level = owning(control, SettingKind::Level);
   if (!level)
      return false;
   const int from = level_position_of(*level, value(*level));
   const int to = level_position_at(*level, fraction);
   if (to != from || persist)
      set(*level, level->values[to], persist);
   if (to != from)
      play_level_sound(to > from);
   paint();
   return true;
}

bool PlayerSettings::handle(const Event& event)
{
   switch (event.kind)
   {
      case RIB_RMLUI_ACTION_SLIDER:
         play_action_sound(event.kind);
         slide(event.id.c_str(), event.fraction, true);
         return true;
      case RIB_RMLUI_ACTION_PART_TOGGLE:
         play_action_sound(event.kind);
         toggle(event.id.c_str());
         return true;
      default:
         return false;
   }
}

bool PlayerSettings::toggle(const char *control)
{
   const SettingDeclaration *setting = owning(control, SettingKind::Switch);
   if (!setting)
      return false;
   const bool on = !switch_on(*setting, value(*setting));
   set(*setting, on != setting->inverted ? 1.0f : 0.0f, true);
   paint();
   return true;
}
}
