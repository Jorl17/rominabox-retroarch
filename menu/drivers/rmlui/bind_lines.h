#ifndef RIB_MENU_BIND_LINES_H
#define RIB_MENU_BIND_LINES_H

#include "host.h"
#include "pad_inputs.h"
#include "../../../input/input_driver.h"
#include "../../../input/input_keymaps.h"
#include <string/stdstring.h>

/* Format the inputs of a bind as we read it (rib_pad_input_effective) in
 * the order KEY, PAD, AXIS, MOUSE, in at most 64 lines. We
 * write a key by its name in RetroArch's config, and a PAD or AXIS input in
 * the form of RetroArch's config (pad_inputs.h), and the menu words both. */
/* One line for each input in a retro_keybind. We read each field separately,
 * because with a comma inside a name, splitting a joined string would be
 * ambiguous. */
#define RIB_BIND_LINE_MAX RIB_HOST_BIND_LINE_MAX

static inline void rib_mouse_label(uint16_t button, char *out, size_t length)
{
   const char *label = NULL;
   switch (button)
   {
#define RIB_MOUSE_BUTTON(value, id, word) case id: label = word; break;
#include "mouse_buttons.inc"
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

static inline void rib_lines_from_bind(const struct retro_keybind *effective,
      char details[][64], char kinds[][8], int *count)
{
   char text[64];

   if (!effective)
      return;
   text[0] = '\0';
   input_keymaps_translate_rk_to_str(RETRO_KEYBIND_KEY(effective), text, sizeof(text));
   if (text[0] && strcmp(text, "nul") != 0)
      rib_push_bind_line(details, kinds, count, "KEY", text);
   if (rib_pad_input_value(effective->joykey, AXIS_NONE, text, sizeof(text)))
      rib_push_bind_line(details, kinds, count, "PAD", text);
   if (rib_pad_input_value(NO_BTN, effective->joyaxis, text, sizeof(text)))
      rib_push_bind_line(details, kinds, count, "AXIS", text);
   if (effective->mbutton != NO_BTN)
   {
      rib_mouse_label(effective->mbutton, text, sizeof(text));
      rib_push_bind_line(details, kinds, count, "MOUSE", text);
   }
}

#endif
