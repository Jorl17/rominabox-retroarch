#include "video_split.h"
#include "../rmlui_shader_mark.h"
#include <stdlib.h>
#include <string.h>

/* Copy the `length` characters of `text` into `out` of `size`, cut to fit. */
static void rib_video_copy(char *out, size_t size, const char *text, size_t length)
{
   if (length >= size)
      length = size - 1;
   memcpy(out, text, length);
   out[length] = '\0';
}

int rib_video_control_read(struct rib_video_control *control,
      const char *preset, const char *text)
{
   const char *cursor = text;
   memset(control, 0, sizeof(*control));
   if (!preset || !*preset || !text)
      return 0;
   rib_video_copy(control->preset, sizeof(control->preset), preset, strlen(preset));
   while (*cursor)
   {
      char word[96];
      const char *colon;
      size_t length = strcspn(cursor, " ");
      rib_video_copy(word, sizeof(word), cursor, length);
      cursor += length;
      while (*cursor == ' ')
         cursor++;
      if (!*word)
         continue;
      colon = strchr(word, ':');
      if (!*control->parameter)
         rib_video_copy(control->parameter, sizeof(control->parameter), word, strlen(word));
      else if (colon && control->count < RIB_VIDEO_CONTROL_POSITIONS)
      {
         control->light[control->count] = (float)strtod(word, NULL);
         control->value[control->count] = (float)strtod(colon + 1, NULL);
         control->count++;
      }
   }
   return control->count >= 2;
}

struct rib_video_split rib_video_split_brightness(
      const struct rib_video_control *controls, unsigned count,
      const char *chosen, float brightness)
{
   struct rib_video_split split;
   const size_t chosen_length = chosen ? strlen(chosen) : 0;
   unsigned index;
   split.parameter = NULL;
   split.value     = 0.0f;
   split.pass      = brightness;
   if (!chosen_length)
      return split;
   for (index = 0; index < count; index++)
   {
      const struct rib_video_control *control = &controls[index];
      const size_t length = strlen(control->preset);
      unsigned at = 1;
      float reached, share;
      if (chosen_length < length || !rib_shader_mark_ends_with(chosen,
               chosen_length, control->preset, length))
         continue;
      split.parameter = control->parameter;
      split.value     = control->value[0];
      if (brightness > 1.0f)
      {
         while (at + 1 < control->count && control->light[at] < brightness)
            at++;
         reached = brightness < control->light[at] ? brightness : control->light[at];
         share   = (reached - control->light[at - 1]) / (control->light[at] - control->light[at - 1]);
         split.value = control->value[at - 1] + share * (control->value[at] - control->value[at - 1]);
         split.pass  = brightness / reached;
      }
      break;
   }
   return split;
}
