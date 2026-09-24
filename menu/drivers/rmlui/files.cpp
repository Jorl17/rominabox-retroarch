#include "files.h"

#include "../../../audio/volume_range.h"
#include <file/file_path.h>
#include <retro_miscellaneous.h>
#include <streams/file_stream.h>
#include <string/stdstring.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#if defined(_WIN32)
#include <windows.h>
#include <encodings/utf.h>
#endif

namespace
{
#if defined(_WIN32)
/* On Windows, rename fails when the destination exists. MoveFileEx replaces it
 * in one step, so after a failed move the old file is still there. */
int platform_replace(const char *from, const char *to)
{
   wchar_t *wide_from = utf8_to_utf16_string_alloc(from);
   wchar_t *wide_to = utf8_to_utf16_string_alloc(to);
   const bool moved = wide_from && wide_to && MoveFileExW(wide_from, wide_to,
         MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
   free(wide_from);
   free(wide_to);
   return moved ? 0 : -1;
}
#else
int platform_replace(const char *from, const char *to)
{
   return filestream_rename(from, to);
}
#endif
rib_rename_step rename_step = platform_replace;

/* One policy for every menu file: move the finished temporary file onto the
 * destination, and never leave the temporary file behind. */
bool replace_file(const char *temporary, const char *path)
{
   if (rename_step(temporary, path) == 0)
      return true;
   filestream_delete(temporary);
   return false;
}

/* We reject a path that is too long for its temporary name, so we never cut
 * it and write to some other file. */
bool temporary_path(char (&temporary)[PATH_MAX_LENGTH], const char *path)
{
   return strlcpy(temporary, path, sizeof(temporary)) < sizeof(temporary)
         && strlcat(temporary, ".tmp", sizeof(temporary)) < sizeof(temporary);
}
}

void rib_files_use_rename(rib_rename_step step)
{
   rename_step = step ? step : platform_replace;
}

bool rib_write_menu_config(config_file_t *config, const char *path)
{
   char temporary[PATH_MAX_LENGTH];
   if (!temporary_path(temporary, path))
      return false;
   if (!config_file_write(config, temporary, true))
      return false;
   return replace_file(temporary, path);
}

bool rib_write_menu_volume(const char *path, float db)
{
   char temporary[PATH_MAX_LENGTH];
   if (!temporary_path(temporary, path))
      return false;
   FILE *file = fopen(temporary, "w");
   if (!file)
      return false;
   fprintf(file, "%s = \"%.1f\"\n", RIB_VOLUME_KEY, db);
   if (fclose(file) != 0)
   {
      filestream_delete(temporary);
      return false;
   }
   return replace_file(temporary, path);
}

/* Update input_libretro_device_p1 in an existing remap, or create one.
 *
 * That key is valid only in a remap file. If we replaced the whole file, as
 * with input_remapping_save_file, we would also write turbo, port and analog
 * settings the author never put there, so we keep the other keys as they are.
 * We update the file in place and never remove it. At launch we copy the
 * author's remap into the data directory only when that file is missing, so
 * without it the next launch would use the author's device again. */
bool rib_write_remap_device(const char *path, unsigned device)
{
   config_file_t *conf;
   char directory[PATH_MAX_LENGTH];
   bool saved;
   char existing[32];
   char wanted[32];
   bool existed;

   if (!path || !*path || !device)
      return false;

   existed = path_is_valid(path);
   conf = existed ? config_file_new_from_path_to_string(path) : NULL;
   if (existed && !conf)
      return false;
   if (!conf && !(conf = config_file_new_alloc()))
      return false;

   snprintf(wanted, sizeof(wanted), "%u", device);
   if (config_get_array(conf, "input_libretro_device_p1",
            existing, sizeof(existing))
         && string_is_equal(existing, wanted))
   {
      config_file_free(conf);
      return true;
   }
   config_set_uint(conf, "input_libretro_device_p1", device);

   fill_pathname_parent_dir(directory, path, sizeof(directory));
   if (*directory && !path_is_directory(directory) && !path_mkdir(directory))
   {
      config_file_free(conf);
      return false;
   }

   saved = rib_write_menu_config(conf, path);
   config_file_free(conf);
   return saved;
}
