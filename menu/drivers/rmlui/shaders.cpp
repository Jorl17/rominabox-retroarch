#include "shaders.hpp"
#include "host.h"
#include "lists.hpp"
#include "words.hpp"
#include "../rmlui_shader_mark.h"
#include "../../../verbosity.h"
#include <file/file_path.h>
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
   /* We write the game's shader with the pass after it in the language of
    * the pass, which we tell by the extension of its preset. */
   std::string pass, written;
   if (!catalog.video_pass.empty() && !assets.empty() && !data.empty())
   {
      pass = assets + "/" + catalog.video_pass;
      written = data + "/" + files::VideoShader + "." + path_get_extension(pass.c_str());
   }
   rib_host_video_pass(pass.c_str(), written.c_str());
   for (int index = 0; index < catalog.count; ++index)
      if (!catalog.entries[index].brightness.empty())
         rib_host_shader_brightness(catalog.entries[index].preset.c_str(),
               catalog.entries[index].brightness.c_str());
}

void Shaders::show_running() const
{
   const char *relatives[RIB_SHADER_MAX];
   if (catalog.count <= 0)
      return;
   for (int index = 0; index < catalog.count; ++index)
      relatives[index] = catalog.entries[index].preset.c_str();
   const int matched = rib_shader_mark_index(rib_host_current_shader(), relatives, catalog.count);
   lists.mark_row(matched < 0 ? "" : catalog.entries[matched].id.c_str(),
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
      if (catalog.entries[index].id == id)
      {
         relative = catalog.entries[index].preset.c_str();
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
      /* We store the id. In the launcher we find the preset wherever it is in
       * this copy of the game. A path would name the folder where the player
       * unpacked the game, which we replace in a new export. */
      snprintf(choice_path, sizeof(choice_path), "%s/%s", data.c_str(), files::ShaderChoice);
      snprintf(body, sizeof(body), "%s\n", id);
      if (!filestream_write_file(choice_path, body, (int64_t)strlen(body)))
         RARCH_ERR("[RIB] the shader is active, but %s could not be written. "
               "The next launch will use the bundled starting shader.\n",
               choice_path);
   }
   show_running();
   return true;
}
}
