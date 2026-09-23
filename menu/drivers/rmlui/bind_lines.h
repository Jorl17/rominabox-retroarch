#ifndef RIB_MENU_BIND_LINES_H
#define RIB_MENU_BIND_LINES_H

#include "host.h"
#include "../../../input/input_driver.h"
#include "../../../input/input_keymaps.h"
#include <string/stdstring.h>

/* Format explicit and autoconfigured inputs in the order KEY, PAD, AXIS,
 * MOUSE. Both binds are arguments, and the output is at most 64 lines. */
/* One line for each input in a retro_keybind. We read each field separately,
 * because with a comma inside a name, splitting a joined string would be
 * ambiguous. */
#define RIB_BIND_LINE_MAX RIB_HOST_BIND_LINE_MAX

static inline void rib_mouse_label(uint16_t button, char *out, size_t length)
{
   const char *label = NULL;
   switch (button)
   {
      case RETRO_DEVICE_ID_MOUSE_LEFT: label = "Left"; break;
      case RETRO_DEVICE_ID_MOUSE_RIGHT: label = "Right"; break;
      case RETRO_DEVICE_ID_MOUSE_MIDDLE: label = "Middle"; break;
      case RETRO_DEVICE_ID_MOUSE_BUTTON_4: label = "Button 4"; break;
      case RETRO_DEVICE_ID_MOUSE_BUTTON_5: label = "Button 5"; break;
      case RETRO_DEVICE_ID_MOUSE_WHEELUP: label = "Wheel up"; break;
      case RETRO_DEVICE_ID_MOUSE_WHEELDOWN: label = "Wheel down"; break;
      case RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELUP: label = "Wheel left"; break;
      case RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELDOWN: label = "Wheel right"; break;
      default: break;
   }
   if (label)
      strlcpy(out, label, length);
   else
      out[0] = '\0';
}

static inline void rib_push_bind_line(char details[][64], char kinds[][8], int *count,
      const char *kind, const char *text)
{
   if (!text || !*text || *count >= RIB_BIND_LINE_MAX)
      return;
   strlcpy(kinds[*count], kind, 8);
   strlcpy(details[*count], text, 64);
   (*count)++;
}

/* For each pad field, the explicit value comes first, and a missing one comes
 * from autoconfig. Both inputs are host data, and we read no live input state. */
static inline const struct retro_keybind *rib_effective_pad(
      const struct retro_keybind *bind,
      const struct retro_keybind *automatic,
      struct retro_keybind *scratch)
{
   if (!bind)
      return NULL;
   *scratch = *bind;
   if (scratch->joykey == NO_BTN)
   {
      scratch->joykey       = automatic->joykey;
      scratch->joykey_label = automatic->joykey_label;
   }
   if (scratch->joyaxis == AXIS_NONE)
   {
      scratch->joyaxis       = automatic->joyaxis;
      scratch->joyaxis_label = automatic->joyaxis_label;
   }
   return scratch;
}

static inline void rib_lines_from_bind(const struct retro_keybind *bind,
      const struct retro_keybind *automatic,
      char details[][64], char kinds[][8], int *count)
{
   struct retro_keybind scratch;
   const struct retro_keybind *effective;
   char text[64];

   if (!bind)
      return;
   effective = rib_effective_pad(bind, automatic, &scratch);
   text[0] = '\0';
   /* There is no autoconfig for the keyboard, so we read the key from the bind. */
   input_keymaps_translate_rk_to_str(bind->key, text, sizeof(text));
   if (text[0] && strcmp(text, "nul") != 0)
      rib_push_bind_line(details, kinds, count, "KEY", text);
   if (effective->joykey != NO_BTN)
   {
      input_config_get_bind_string_joykey(false, text, "", effective,
            sizeof(text));
      rib_push_bind_line(details, kinds, count, "PAD", text);
   }
   if (effective->joyaxis != AXIS_NONE)
   {
      input_config_get_bind_string_joyaxis(false, text, "", effective,
            sizeof(text));
      rib_push_bind_line(details, kinds, count, "AXIS", text);
   }
   if (bind->mbutton != NO_BTN)
   {
      rib_mouse_label(bind->mbutton, text, sizeof(text));
      rib_push_bind_line(details, kinds, count, "MOUSE", text);
   }
}

#endif
