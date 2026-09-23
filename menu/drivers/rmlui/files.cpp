#include "files.h"

#include "../../../audio/volume_range.h"
#include <file/file_path.h>
#include <retro_miscellaneous.h>
#include <streams/file_stream.h>
#include <string/stdstring.h>
#include <cstdio>

namespace
{
enum class Replacement
{
   Controls,
   Remap,
   Volume
};

bool replace_file(const char *temporary, const char *path, Replacement policy)
{
#if defined(_WIN32)
   if (policy != Replacement::Controls && filestream_exists(path))
      filestream_delete(path);
#endif
   const int result = policy == Replacement::Remap
         ? filestream_rename(temporary, path) : rename(temporary, path);
   if (result == 0)
      return true;
   if (policy != Replacement::Controls)
      filestream_delete(temporary);
   return false;
}
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
