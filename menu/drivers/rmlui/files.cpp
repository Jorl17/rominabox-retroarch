#include "files.h"

#include "../../../audio/volume_range.h"
#include <file/file_path.h>
#include <retro_miscellaneous.h>
#include <streams/file_stream.h>
#include <string/stdstring.h>
#include <cstdio>
#include <cstring>

namespace
{
enum class Replacement
{
   Controls,
   Remap,
   Volume
};

#if defined(_WIN32)
bool rename_replaces_existing = false;
#else
bool rename_replaces_existing = true;
#endif
rib_rename_step rename_step = nullptr;

bool replace_file(const char *temporary, const char *path, Replacement policy)
{
   if (!rename_replaces_existing
         && policy != Replacement::Controls && filestream_exists(path))
      filestream_delete(path);
   const int result = rename_step ? rename_step(temporary, path)
         : policy == Replacement::Remap
         ? filestream_rename(temporary, path) : rename(temporary, path);
   if (result == 0)
      return true;
   if (policy != Replacement::Controls)
      filestream_delete(temporary);
   return false;
}
}

void rib_files_use_rename(rib_rename_step step, bool replaces_existing)
{
   rename_step = step;
#if defined(_WIN32)
   rename_replaces_existing = step ? replaces_existing : false;
#else
   rename_replaces_existing = step ? replaces_existing : true;
#endif
}

bool rib_write_menu_config(config_file_t *config, const char *path,
      enum rib_config_write_policy policy)
{
   char temporary[PATH_MAX_LENGTH];
   if (policy == RIB_CONFIG_WRITE_REMAP)
   {
      if (strlcpy(temporary, path, sizeof(temporary)) >= sizeof(temporary)
            || strlcat(temporary, ".tmp", sizeof(temporary)) >= sizeof(temporary))
         return false;
   }
   else
      snprintf(temporary, sizeof(temporary), "%s.tmp", path);
   if (!config_file_write(config, temporary, true))
      return false;
   return replace_file(temporary, path, policy == RIB_CONFIG_WRITE_REMAP
         ? Replacement::Remap : Replacement::Controls);
}

bool rib_write_menu_volume(const char *path, float db)
{
   char temporary[PATH_MAX_LENGTH];
   snprintf(temporary, sizeof(temporary), "%s.tmp", path);
   FILE *file = fopen(temporary, "w");
   if (!file)
      return false;
   fprintf(file, "%s = \"%.1f\"\n", RIB_VOLUME_KEY, db);
   if (fclose(file) != 0)
   {
      filestream_delete(temporary);
      return false;
   }
   return replace_file(temporary, path, Replacement::Volume);
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

   saved = rib_write_menu_config(conf, path, RIB_CONFIG_WRITE_REMAP);
   config_file_free(conf);
   return saved;
}
