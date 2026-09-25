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
/* Where we store the position of a switch, in the game's storage. */
static bool rib_toggle_path(const char *data, const char *id, char *out, size_t length)
{
   if (!data || !*data || !id || !*id)
      return false;
   snprintf(out, length, "%s/toggle-%s", data, id);
   return true;
}

static void rib_toggle_remember(const rib_toggle_t *toggle, const char *data)
{
   char path[PATH_MAX_LENGTH];
   const char *body = toggle->state ? "1\n" : "0\n";

   if (!rib_toggle_path(data, toggle->id, path, sizeof(path)))
      return;
   if (!filestream_write_file(path, body, (int64_t)strlen(body)))
      RARCH_ERR("[RIB] the switch '%s' is %s, but %s could not be written, so "
            "the next launch will start from the design's default.\n",
            toggle->id, toggle->state ? "on" : "off", path);
}

static bool rib_toggle_recall(rib_toggle_t *toggle, const char *data)
{
   char path[PATH_MAX_LENGTH];
   int64_t length = 0;
   char *body = NULL;

   if (!rib_toggle_path(data, toggle->id, path, sizeof(path)))
      return false;
   if (!filestream_read_file(path, (void**)&body, &length) || !body)
      return false;
   toggle->state = length > 0 && body[0] == '1';
   free(body);
   return true;
}

}

void Toggles::load(const rib_design_data& design, const char *data)
{
   count = design.toggle_count;
   for (int index = 0; index < count; ++index)
   {
      entries[index] = design.toggles[index];
      rib_toggle_recall(&entries[index], data);
   }
}

/* The combined effect of all switches. We combine them instead of applying
 * them in turn, so of two switches that lock the slots, the last does not win. */
void Toggles::apply() const
{
   const rib_toggle_t *guarding = NULL;
   int index;

   for (index = 0; index < count; ++index)
   {
      const rib_toggle_t *toggle = &entries[index];
      lists.set_toggle(toggle->id,
            toggle->state ? toggle->on : toggle->off, toggle->state);
      if (toggle->state && toggle->guard == RIB_TOGGLE_GUARD_SAVES && !guarding)
         guarding = toggle;
   }
   slots.guard_slots(guarding ? guarding->guard_label : NULL,
         guarding ? guarding->guard_status : NULL);
}

void Toggles::toggle(const char *id, const char *data)
{
   if (!id || !*id)
      return;
   for (int index = 0; index < count; ++index)
   {
      auto& entry = entries[index];
      if (!string_is_equal(entry.id, id))
         continue;
      entry.state = !entry.state;
      rib_toggle_remember(&entry, data);
      apply();
      break;
   }
}

namespace {
int last_position(const rib_setting_declaration& level)
{
   return level.positions - 1;
}

/* The position of a level nearest to `value`. */
int position_of(const rib_setting_declaration& level, float value)
{
   float fraction = (value - level.low) / (level.high - level.low);
   if (fraction < 0.0f) fraction = 0.0f;
   if (fraction > 1.0f) fraction = 1.0f;
   return (int)(fraction * (float)last_position(level) + 0.5f);
}

/* We use the declared values exactly at both ends, not a sum of steps. */
float value_at(const rib_setting_declaration& level, int position)
{
   if (position <= 0) return level.low;
   if (position >= last_position(level)) return level.high;
   return level.low + (level.high - level.low) * (float)position
         / (float)last_position(level);
}

float fraction_at(const rib_setting_declaration& level, int position)
{
   return (float)position / (float)last_position(level);
}

bool switched_on(const rib_setting_declaration& setting, float value)
{
   return (value != 0.0f) != setting.inverted;
}
}

void PlayerSettings::load(const rib_design_data& design, const char *data_directory)
{
   count = design.setting_count;
   for (int index = 0; index < count; ++index)
      entries[index] = design.settings[index];
   data[0] = '\0';
   if (data_directory && *data_directory)
      strlcpy(data, data_directory, sizeof(data));
}

const rib_setting_declaration *PlayerSettings::owning(const char *control,
      enum rib_setting_kind kind) const
{
   if (!control || !*control)
      return nullptr;
   for (int index = 0; index < count; ++index)
      if (entries[index].kind == kind && string_is_equal(entries[index].control, control))
         return &entries[index];
   return nullptr;
}

float PlayerSettings::value(const rib_setting_declaration& setting) const
{
   float current = setting.kind == RIB_SETTING_LEVEL ? setting.high : 0.0f;
   rib_host_setting(setting.key, &current);
   return current;
}

void PlayerSettings::attach()
{
   for (int index = 0; index < count; ++index)
   {
      const rib_setting_declaration& setting = entries[index];
      if (setting.kind != RIB_SETTING_LEVEL)
         continue;
      parts.set_slider_step(setting.control, 1.0f / (float)last_position(setting));
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
   for (int index = 0; index < count; ++index)
   {
      const rib_setting_declaration& setting = entries[index];
      const float current = value(setting);
      if (setting.kind == RIB_SETTING_LEVEL)
         parts.set_slider(setting.control,
               fraction_at(setting, position_of(setting, current)), "");
      else
      {
         const bool on = switched_on(setting, current);
         lists.set_toggle(setting.control, on ? setting.on : setting.off, on);
      }
   }
}

void PlayerSettings::set(const rib_setting_declaration& setting, float chosen, bool persist)
{
   char path[PATH_MAX_LENGTH];
   char text[32];

   if (!rib_host_set_setting(setting.key, chosen))
   {
      RARCH_ERR("[RIB] the setting '%s' drives '%s', which this player cannot "
            "change while the game runs.\n", setting.id, setting.key);
      return;
   }
   if (!persist || !data[0])
      return;
   if (setting.kind == RIB_SETTING_LEVEL)
      snprintf(text, sizeof(text), "%.1f", chosen);
   else
      strlcpy(text, chosen != 0.0f ? "true" : "false", sizeof(text));
   snprintf(path, sizeof(path), "%s/%s", data, setting.file);
   if (!rib_write_player_setting(path, setting.key, text))
      RARCH_ERR("[RIB] '%s' is %s now, but %s could not be written, so the "
            "next launch will start from the export's default.\n",
            setting.id, text, path);
}

bool PlayerSettings::slide(const char *control, float fraction, bool persist)
{
   const rib_setting_declaration *level = owning(control, RIB_SETTING_LEVEL);
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

bool PlayerSettings::toggle(const char *control)
{
   const rib_setting_declaration *setting = owning(control, RIB_SETTING_SWITCH);
   if (!setting)
      return false;
   const bool on = !switched_on(*setting, value(*setting));
   set(*setting, on != setting->inverted ? 1.0f : 0.0f, true);
   paint();
   return true;
}
}
