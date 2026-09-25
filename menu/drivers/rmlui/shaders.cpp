#include "shaders.hpp"
#include "host.h"
#include "document.hpp"
#include "lists.hpp"
#include "parts.hpp"
#include "slots.hpp"
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
   const char *current;
   int index;
   int row;
   int rows;
   int matched = -1;
   bool ours = false;

   if (catalog.count <= 0)
      return;
   rows = lists.visible_row_count();
   for (row = 0; row < rows && !ours; ++row)
   {
      const char *id = lists.list_row_id(row);

      for (index = 0; index < catalog.count; ++index)
         if (id && string_is_equal(id, catalog.entries[index].id))
            ours = true;
   }
   if (!ours)
      return;

   for (index = 0; index < catalog.count; ++index)
      relatives[index] = catalog.entries[index].preset;
   current = rib_host_current_shader();
   matched = rib_shader_mark_index(current, relatives, catalog.count);
   if (matched < 0)
   {
      fprintf(stderr, "[RIB] no bundled shader matches the one running: %s\n",
            current && current[0] ? current : "none");
      lists.mark_row("", catalog.state_on, catalog.state_off);
      return;
   }
   fprintf(stderr, "[RIB] shader row '%s' is the one running\n", catalog.entries[matched].id);
   lists.mark_row(catalog.entries[matched].id,
         catalog.state_on, catalog.state_off);
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
