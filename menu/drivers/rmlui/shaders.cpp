#include "shaders.hpp"
#include "host.h"
#include "../rmlui_bridge.h"
#include "../rmlui_shader_mark.h"
#include "../../../verbosity.h"
#include <file/config_file.h>
#include <streams/file_stream.h>
#include <string/stdstring.h>
#include <cstdio>
#include <cstring>

namespace rib {
void Shaders::load(const char *asset_directory)
{
   char path[PATH_MAX_LENGTH];
   config_file_t *config;
   char list[1024];
   char *cursor;
   char *token;

   count = 0;
   state_on[0] = '\0';
   state_off[0] = '\0';
   if (!asset_directory || !*asset_directory)
      return;
   snprintf(path, sizeof(path), "%s/shaders.cfg", asset_directory);
   if (!(config = config_file_new_from_path_to_string(path)))
      return;
   config_get_array(config, "shader_state_on", state_on, sizeof(state_on));
   config_get_array(config, "shader_state_off", state_off, sizeof(state_off));
   if (!config_get_array(config, "shader_ids", list, sizeof(list)))
   {
      config_file_free(config);
      return;
   }
   cursor = list;
   while ((token = strtok_r(cursor, " ", &cursor)))
   {
      char key[96];
      char preset[PATH_MAX_LENGTH];

      if (!*token)
         continue;
      if (count >= Max)
      {
         RARCH_ERR("[RIB] shader list has more than %d entries; the rest "
               "are not offered.\n", Max);
         break;
      }
      strlcpy(ids[count], token, sizeof(ids[count]));
      snprintf(key, sizeof(key), "shader_preset_%s", token);
      preset[0] = '\0';
      config_get_array(config, key, preset, sizeof(preset));
      strlcpy(presets[count], preset, sizeof(presets[count]));
      ++count;
   }
   config_file_free(config);
}

void Shaders::show_running() const
{
   const char *relatives[Max];
   const char *current;
   int index;
   int row;
   int rows;
   int matched = -1;
   bool ours = false;

   if (count <= 0)
      return;
   rows = rib_rmlui_visible_row_count();
   for (row = 0; row < rows && !ours; ++row)
   {
      const char *id = rib_rmlui_list_row_id(row);

      for (index = 0; index < count; ++index)
         if (id && string_is_equal(id, ids[index]))
            ours = true;
   }
   if (!ours)
      return;

   for (index = 0; index < count; ++index)
      relatives[index] = presets[index];
   current = rib_host_current_shader();
   matched = rib_shader_mark_index(current, relatives, count);
   if (matched < 0)
   {
      fprintf(stderr, "[RIB] no bundled shader matches the one running: %s\n",
            current && current[0] ? current : "none");
      rib_rmlui_mark_row("", state_on, state_off);
      return;
   }
   fprintf(stderr, "[RIB] shader row '%s' is the one running\n", ids[matched]);
   rib_rmlui_mark_row(ids[matched], state_on, state_off);
}

bool Shaders::apply(const char *id, const char *assets, const char *data) const
{
   char absolute[PATH_MAX_LENGTH];
   char choice_path[PATH_MAX_LENGTH];
   char body[PATH_MAX_LENGTH + 2];
   const char *relative = NULL;
   int index;
   bool known = false;

   if (!id || !*id || !rib_host_has_settings())
      return false;
   for (index = 0; index < count; ++index)
      if (string_is_equal(ids[index], id))
      {
         relative = presets[index];
         known = true;
         break;
      }
   /* Other generated lists use this action too, so an unknown id is for them. */
   if (!known)
      return false;

   absolute[0] = '\0';
   if (relative && *relative && assets && *assets)
      snprintf(absolute, sizeof(absolute), "%s/%s", assets, relative);

   rib_host_apply_shader(id, absolute);

   if (data && *data)
   {
      snprintf(choice_path, sizeof(choice_path), "%s/shader-choice", data);
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
