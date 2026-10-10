#include "pad_inputs.h"

#include "../../../configuration.h"
#include "../../../input/input_driver.h"
#include "../../../input/input_types.h"
#include "../../menu_driver.h"
#include <string/stdstring.h>
#include <stdlib.h>

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

bool rib_pad_input_down(unsigned bind)
{
   input_driver_state_t *input_st = input_state_get_ptr();
   settings_t *settings           = config_get_ptr();
   unsigned pads[MAX_USERS + 1];
   unsigned index;

   if (!input_st || !settings || bind >= RARCH_BIND_LIST_END
         || !rib_pad_input_id(bind))
      return false;
   player_one_pads(pads);
   for (index = 0; pads[index] < MAX_USERS; ++index)
   {
      const float threshold = settings->floats.input_axis_threshold;
      const struct retro_keybind *bound = &input_autoconf_binds[pads[index]][bind];
      if (held_by(input_st->primary_joypad, pads[index], bound, threshold)
#ifdef HAVE_MFI
            || held_by(input_st->secondary_joypad, pads[index], bound, threshold)
#endif
         )
         return true;
   }
   return false;
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
