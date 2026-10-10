#include "pad_inputs.h"

#include "../../../configuration.h"
#include "../../../input/input_driver.h"
#include "../../../input/input_types.h"
#include "../../menu_driver.h"
#include <compat/strl.h>
#include <string/stdstring.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Home, as declared in hotkeys.inc: its id, and the RetroArch bind for the
 * menu button in the profile of a pad. */
#define RIB_HOTKEY_PAD_HOME(id, bind, word) static const char home_id[] = id, home_bind[] = bind;
#include "hotkeys.inc"

/* A position of the standard pad is one of the sixteen RetroPad binds, which
 * come first in the RetroArch bind table. */
static bool is_position(unsigned bind)
{
   return bind < RARCH_FIRST_CUSTOM_BIND;
}

/* The bind named `base` anywhere in the RetroArch bind table. */
static bool bind_named(const char *base, unsigned *bind)
{
   unsigned index;
   for (index = 0; index < RARCH_BIND_LIST_END; ++index)
   {
      const struct input_bind_map *entry = INPUT_CONFIG_BIND_MAP_GET(index);
      if (entry && entry->valid && entry->base && string_is_equal(entry->base, base))
      {
         *bind = index;
         return true;
      }
   }
   return false;
}

static bool is_home(unsigned bind)
{
   unsigned home;
   return bind_named(home_bind, &home) && bind == home;
}

bool rib_pad_input_bind(const char *id, unsigned *bind)
{
   unsigned found;
   if (!id || !*id || !bind)
      return false;
   if (string_is_equal(id, home_id))
      return bind_named(home_bind, bind);
   if (!bind_named(id, &found) || !is_position(found))
      return false;
   *bind = found;
   return true;
}

const char *rib_pad_input_id(unsigned bind)
{
   if (is_home(bind))
      return home_id;
   if (!is_position(bind))
      return NULL;
   return INPUT_CONFIG_BIND_MAP_GET(bind)->base;
}

/* The pad of the first player: its joypad index in RetroArch, and the binds
 * from its profile. */
static unsigned first_pad(void)
{
   settings_t *settings = config_get_ptr();
   return settings ? settings->uints.input_joypad_index[0] : 0;
}

static bool held_by(const input_device_driver_t *joypad, unsigned pad,
      const struct retro_keybind *bound, float threshold)
{
   if (!joypad)
      return false;
   if (bound->joykey != NO_BTN && joypad->button(pad, bound->joykey))
      return true;
   return bound->joyaxis != AXIS_NONE
         && ((float)abs(joypad->axis(pad, bound->joyaxis)) / 0x8000) > threshold;
}

/* The joypad index of every pad that plays as player 1, ended by MAX_USERS.
 * That is each port below input_max_users that is mapped to player 1 in the
 * remap (input_remap_port_pN), through the joypad index of that port in
 * RetroArch. */
static void player_one_pads(unsigned *pads)
{
   settings_t *settings = config_get_ptr();
   unsigned count       = 0;
   unsigned index;

   if (settings)
   {
      const unsigned *ports = settings->uints.input_remap_port_map[0];
      for (index = 0; index < MAX_USERS && ports[index] < MAX_USERS; ++index)
      {
         const unsigned pad = settings->uints.input_joypad_index[ports[index]];
         if (ports[index] < settings->uints.input_max_users && pad < MAX_USERS)
            pads[count++] = pad;
      }
   }
   pads[count] = MAX_USERS;
}

/* Whether `input`, or else the bind `bind` in the profile of each pad, is
 * down on any pad that plays as player 1. */
static bool down_on_player_one(const struct retro_keybind *input, unsigned bind)
{
   input_driver_state_t *input_st = input_state_get_ptr();
   settings_t *settings           = config_get_ptr();
   unsigned pads[MAX_USERS + 1];
   unsigned index;

   if (!input_st || !settings)
      return false;
   player_one_pads(pads);
   for (index = 0; pads[index] < MAX_USERS; ++index)
   {
      const float threshold = settings->floats.input_axis_threshold;
      const struct retro_keybind *bound = input ? input : &input_autoconf_binds[pads[index]][bind];
      if (held_by(input_st->primary_joypad, pads[index], bound, threshold)
#ifdef HAVE_MFI
            || held_by(input_st->secondary_joypad, pads[index], bound, threshold)
#endif
         )
         return true;
   }
   return false;
}

bool rib_pad_input_down(unsigned bind)
{
   if (bind >= RARCH_BIND_LIST_END || !rib_pad_input_id(bind))
      return false;
   return down_on_player_one(NULL, bind);
}

bool rib_pad_input_value_down(const char *value)
{
   struct retro_keybind input;
   memset(&input, 0, sizeof(input));
   if (!rib_pad_input_parse(value, &input.joykey, &input.joyaxis))
      return false;
   return down_on_player_one(&input, 0);
}

bool rib_pad_input_of(uint16_t joykey, uint32_t joyaxis, unsigned *bind)
{
   return rib_pad_input_on(first_pad(), joykey, joyaxis, bind);
}

bool rib_pad_input_on(unsigned pad, uint16_t joykey, uint32_t joyaxis, unsigned *bind)
{
   unsigned home;
   unsigned index;

   if (!bind || pad >= MAX_USERS || (joykey == NO_BTN && joyaxis == AXIS_NONE))
      return false;
   for (index = 0; index < RARCH_FIRST_CUSTOM_BIND; ++index)
   {
      const struct retro_keybind *bound = &input_autoconf_binds[pad][index];
      if ((joykey != NO_BTN && bound->joykey == joykey)
            || (joyaxis != AXIS_NONE && bound->joyaxis == joyaxis))
      {
         *bind = index;
         return true;
      }
   }
   if (joykey != NO_BTN && bind_named(home_bind, &home)
         && input_autoconf_binds[pad][home].joykey == joykey)
   {
      *bind = home;
      return true;
   }
   return false;
}

bool rib_pad_input_value(uint16_t joykey, uint32_t joyaxis, char *value, size_t size)
{
   const char *direction = "";

   if (!value || !size)
      return false;
   if (joyaxis != AXIS_NONE)
   {
      if (AXIS_NEG_GET(joyaxis) != AXIS_DIR_NONE)
         snprintf(value, size, "-%u", (unsigned)AXIS_NEG_GET(joyaxis));
      else
         snprintf(value, size, "+%u", (unsigned)AXIS_POS_GET(joyaxis));
      return true;
   }
   if (joykey == NO_BTN)
      return false;
   if (!GET_HAT_DIR(joykey))
   {
      snprintf(value, size, "%u", (unsigned)joykey);
      return true;
   }
   switch (GET_HAT_DIR(joykey))
   {
      case HAT_UP_MASK:    direction = "up";    break;
      case HAT_DOWN_MASK:  direction = "down";  break;
      case HAT_LEFT_MASK:  direction = "left";  break;
      case HAT_RIGHT_MASK: direction = "right"; break;
      default: break;
   }
   snprintf(value, size, "h%u%s", (unsigned)GET_HAT(joykey), direction);
   return true;
}

bool rib_pad_input_parse(const char *value, uint16_t *joykey, uint32_t *joyaxis)
{
   static const struct { const char *word; uint16_t mask; } directions[] = {
      {"up", HAT_UP_MASK}, {"down", HAT_DOWN_MASK},
      {"left", HAT_LEFT_MASK}, {"right", HAT_RIGHT_MASK},
   };
   char *end = NULL;
   unsigned long number;
   size_t index;

   if (!value || !joykey || !joyaxis)
      return false;
   *joykey  = NO_BTN;
   *joyaxis = AXIS_NONE;
   if ((value[0] == '+' || value[0] == '-') && isdigit((unsigned char)value[1]))
   {
      number = strtoul(value + 1, &end, 10);
      if (*end || number >= AXIS_DIR_NONE)
         return false;
      *joyaxis = value[0] == '+' ? AXIS_POS(number) : AXIS_NEG(number);
      return true;
   }
   if (value[0] == 'h' && isdigit((unsigned char)value[1]))
   {
      number = strtoul(value + 1, &end, 10);
      for (index = 0; index < sizeof(directions) / sizeof(directions[0]); ++index)
         if (string_is_equal(end, directions[index].word))
         {
            *joykey = HAT_MAP(number, directions[index].mask);
            return true;
         }
      return false;
   }
   if (!isdigit((unsigned char)value[0]))
      return false;
   number = strtoul(value, &end, 10);
   if (*end || number >= NO_BTN)
      return false;
   *joykey = (uint16_t)number;
   return true;
}

bool rib_pad_input_name(unsigned bind, char *name, size_t size)
{
   const unsigned pad = first_pad();
   const struct retro_keybind *bound;
   const struct input_bind_label *label;
   const char *text = NULL;

   if (!name || !size || pad >= MAX_USERS || bind >= RARCH_BIND_LIST_END)
      return false;
   bound = &input_autoconf_binds[pad][bind];
   label = &input_autoconf_bind_labels[pad][bind];
   if (bound->joykey != NO_BTN && label->joykey && *label->joykey)
      text = label->joykey;
   else if (bound->joyaxis != AXIS_NONE && label->joyaxis && *label->joyaxis)
      text = label->joyaxis;
   if (!text)
      return false;
   strlcpy(name, text, size);
   return true;
}

static bool named_on(unsigned pad, uint16_t joykey, uint32_t joyaxis)
{
   unsigned bind;
   return rib_pad_input_on(pad, joykey, joyaxis, &bind);
}

/* We capture from every pad that plays as player 1, each through its own
 * profile. */
static struct menu_rib_capture_pads capture_pads(void)
{
   struct menu_rib_capture_pads pads;
   player_one_pads(pads.pads);
   pads.named = named_on;
   return pads;
}

bool rib_pad_input_capture_start(struct retro_keybind *output, unsigned seconds)
{
   const struct menu_rib_capture_pads pads = capture_pads();
   return menu_input_rib_capture_start(output, &pads, seconds);
}

bool rib_pad_input_bind_start(unsigned index, unsigned seconds)
{
   const struct menu_rib_capture_pads pads = capture_pads();
   return menu_input_rib_bind_start(index, &pads, seconds);
}

unsigned rib_pad_input_captured_pad(void)
{
   return menu_input_rib_captured_pad();
}

bool rib_pad_input_binds_conflict(unsigned left, unsigned right)
{
   const struct retro_keybind *changed = &input_config_binds[0][left];
   const struct retro_keybind *candidate = &input_config_binds[0][right];
   return (RETRO_KEYBIND_KEY(changed) != RETROK_UNKNOWN
            && RETRO_KEYBIND_KEY(changed) == RETRO_KEYBIND_KEY(candidate)) ||
          (changed->joykey != NO_BTN && changed->joykey == candidate->joykey) ||
          (changed->joyaxis != AXIS_NONE && changed->joyaxis == candidate->joyaxis) ||
          (changed->mbutton != NO_BTN && changed->mbutton == candidate->mbutton);
}
