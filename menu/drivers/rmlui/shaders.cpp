#include "shaders.hpp"
#include "host.h"
#include "lists.hpp"
#include "words.hpp"
#include "../rmlui_shader_mark.h"
#include "../../../verbosity.h"
#include <streams/file_stream.h>
#include <string/stdstring.h>
#include <cstdio>
#include <cstring>

namespace rib {
void Shaders::load(const char *asset_directory, const char *data_directory)
{
   rib_load_shaders(asset_directory, &catalog);
   assets = asset_directory ? asset_directory : "";
   data = data_directory ? data_directory : "";
}

void Shaders::show_running() const
{
   const char *relatives[RIB_SHADER_MAX];
   if (catalog.count <= 0)
      return;
   for (int index = 0; index < catalog.count; ++index)
      relatives[index] = catalog.entries[index].preset;
   const int matched = rib_shader_mark_index(rib_host_current_shader(), relatives, catalog.count);
   lists.mark_row(matched < 0 ? "" : catalog.entries[matched].id,
         say(Word::ShaderMark).c_str(), "");
}

bool Shaders::choose(const char *id)
{
   char absolute[PATH_MAX_LENGTH];
   char choice_path[PATH_MAX_LENGTH];
   char body[PATH_MAX_LENGTH + 2];
   const char *relative = NULL;
   int index;
   bool known = false;

   if (!id || !*id || !rib_host_has_settings())
      return false;
   for (index = 0; index < catalog.count; ++index)
      if (string_is_equal(catalog.entries[index].id, id))
      {
         relative = catalog.entries[index].preset;
         known = true;
         break;
      }
   if (!known)
      return false;

   absolute[0] = '\0';
   if (relative && *relative && !assets.empty())
      snprintf(absolute, sizeof(absolute), "%s/%s", assets.c_str(), relative);

   rib_host_apply_shader(id, absolute);

   if (!data.empty())
   {
      snprintf(choice_path, sizeof(choice_path), "%s/shader-choice", data.c_str());
      if (absolute[0])
         snprintf(body, sizeof(body), "%s\n", absolute);
      else
         strlcpy(body, "\n", sizeof(body));
      if (!filestream_write_file(choice_path, body, (int64_t)strlen(body)))
         RARCH_ERR("[RIB] the shader is active, but %s could not be written. "
               "The next launch will use the bundled starting shader.\n",
               choice_path);
   }
   show_running();
   return true;
}
}
