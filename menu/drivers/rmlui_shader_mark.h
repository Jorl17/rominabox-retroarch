/* Which bundled row is the shader running in RetroArch.
 *
 * We stage the menu document with the author's starting preset marked on.
 * After a restart that filter runs only until the player picks another one.
 * After that, the preset we gave RetroArch is running, and the staged mark is
 * out of date. We mark the row from the path of the running preset, because a
 * second record of the choice could differ from the picture.
 */
#ifndef RMLUI_SHADER_MARK_H__
#define RMLUI_SHADER_MARK_H__

#include <string.h>
#include <file/file_path.h>

/* Whether `current` ends with `relative`, with any separator matching any
 * other. Windows has two, and a path joined there can contain both. */
static int rib_shader_mark_ends_with(const char *current, size_t current_len,
      const char *relative, size_t relative_len)
{
   size_t offset;
   const char *tail = current + current_len - relative_len;

   for (offset = 0; offset < relative_len; ++offset)
      if (tail[offset] != relative[offset]
            && !(PATH_CHAR_IS_SLASH(tail[offset]) && PATH_CHAR_IS_SLASH(relative[offset])))
         return 0;
   return 1;
}

static int rib_shader_mark_index(const char *current,
      const char *const *relatives, int count)
{
   int index;
   int unfiltered = -1;
   int found      = -1;
   int running    = current && current[0];

   if (!relatives || count <= 0)
      return -1;

   for (index = 0; index < count; ++index)
   {
      const char *relative = relatives[index];
      size_t relative_len;
      size_t current_len;

      if (!relative || !relative[0])
      {
         unfiltered = index;
         continue;
      }
      if (!running)
         continue;
      relative_len = strlen(relative);
      current_len  = strlen(current);
      /* At launch we pass the menu-assets path plus this relative preset. We
       * match the end of that path and keep no copy of it elsewhere. With a
       * separator before it, one preset name cannot match the end of another. */
      if (current_len >= relative_len + 1
            && PATH_CHAR_IS_SLASH(current[current_len - relative_len - 1])
            && rib_shader_mark_ends_with(current, current_len, relative, relative_len))
      {
         found = index;
         break;
      }
   }
   if (found < 0 && !running)
      found = unfiltered;
   return found;
}

#endif
