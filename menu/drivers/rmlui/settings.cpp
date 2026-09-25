#include "settings.hpp"
#include "host.h"
#include "files.h"
#include "document.hpp"
#include "lists.hpp"
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
int last_position(const SettingDeclaration& level)
{
   return level.positions - 1;
}

/* The position of a level nearest to `value`. */
int position_of(const SettingDeclaration& level, float value)
{
   float fraction = (value - level.low) / (level.high - level.low);
   if (fraction < 0.0f) fraction = 0.0f;
   if (fraction > 1.0f) fraction = 1.0f;
   return (int)(fraction * (float)last_position(level) + 0.5f);
}

/* We use the declared values exactly at both ends, not a sum of steps. */
float value_at(const SettingDeclaration& level, int position)
{
   if (position <= 0) return level.low;
   if (position >= last_position(level)) return level.high;
   return level.low + (level.high - level.low) * (float)position
         / (float)last_position(level);
}

float fraction_at(const SettingDeclaration& level, int position)
{
   return (float)position / (float)last_position(level);
}

bool switched_on(const SettingDeclaration& setting, float value)
{
   return (value != 0.0f) != setting.inverted;
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
   float current = setting.kind == SettingKind::Level ? setting.high : 0.0f;
   rib_host_setting(setting.key.c_str(), &current);
   return current;
}

void PlayerSettings::attach()
{
   for (const SettingDeclaration& setting : entries)
   {
      if (setting.kind != SettingKind::Level)
         continue;
      parts.set_slider_step(setting.control.c_str(), 1.0f / (float)last_position(setting));
      /* We move a level between positions, from a file or a hotkey, to the
       * nearest one and store it there. */
      const float current = value(setting);
      const float nearest = value_at(setting, position_of(setting, current));
      if (nearest != current)
         set(setting, nearest, true);
   }
   paint();
}

void PlayerSettings::paint() const
{
   for (const SettingDeclaration& setting : entries)
   {
      const float current = value(setting);
      if (setting.kind == SettingKind::Level)
         parts.set_slider(setting.control.c_str(),
               fraction_at(setting, position_of(setting, current)), "");
      else
      {
         const bool on = switched_on(setting, current);
         lists.set_toggle(setting.control.c_str(), (on ? setting.on : setting.off).c_str(), on);
      }
   }
}

void PlayerSettings::set(const SettingDeclaration& setting, float chosen, bool persist)
{
   char text[32];

   if (!rib_host_set_setting(setting.key.c_str(), chosen))
   {
      RARCH_ERR("[RIB] the setting '%s' drives '%s', which this player cannot "
            "change while the game runs.\n", setting.id.c_str(), setting.key.c_str());
      return;
   }
   if (!persist || data.empty())
      return;
   if (setting.kind == SettingKind::Level)
      snprintf(text, sizeof(text), "%.1f", chosen);
   else
      strlcpy(text, chosen != 0.0f ? "true" : "false", sizeof(text));
   const std::string path = data + "/" + setting.file;
   if (!rib_write_player_setting(path.c_str(), setting.key.c_str(), text))
      RARCH_ERR("[RIB] '%s' is %s now, but %s could not be written, so the "
            "next launch will start from the export's default.\n",
            setting.id.c_str(), text, path.c_str());
}

bool PlayerSettings::slide(const char *control, float fraction, bool persist)
{
   const SettingDeclaration *level = owning(control, SettingKind::Level);
   if (!level)
      return false;
   const int from = position_of(*level, value(*level));
   const int to = position_of(*level, level->low + fraction * (level->high - level->low));
   if (to != from || persist)
      set(*level, value_at(*level, to), persist);
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
   const bool on = !switched_on(*setting, value(*setting));
   set(*setting, on != setting->inverted ? 1.0f : 0.0f, true);
   paint();
   return true;
}
}
