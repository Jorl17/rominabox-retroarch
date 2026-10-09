/* DATA on macOS: the system's save and open panels, which give the game, in
 * its sandbox, the one file the player chooses
 * (com.apple.security.files.user-selected.read-write), and the zips of the
 * game's data (game_data.h, which the player build compiles from the
 * ROM-in-a-Box sources it is handed). */
#import <AppKit/AppKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#include <cstdio>
#include <string>

#include "game_data_host.h"
#include "game_data.h"
#include "../../../rominabox_environment.h"

/* The launcher's file layer is C, with no linkage of its own for C++. */
extern "C" {
#include "portable_fs.h"
}

namespace {
/* The zip the player chose to import, until they confirm or choose again. */
std::string chosen;

/* The game's data folder, or empty. */
std::string data_folder()
{
   char *path = rib_data_directory();
   std::string folder = path ? path : "";
   free(path);
   return folder;
}

UTType *zip_type()
{
   return [UTType typeWithFilenameExtension:@"zip"];
}
}

rib_data_answer rib_game_data_export_to_file(char *sentence, size_t sentence_size)
{
   @autoreleasepool {
      const std::string folder = data_folder();
      rib_game_t *game = rib_games_new(1);
      char name[RIB_GAME_DATA_TEXT_SIZE] = "Game data.zip";
      const char *folders[1] = {folder.c_str()};
      if (game && rib_game_manifest_read(folder.c_str(), game) == 0)
         rib_game_data_file_name(game, name, sizeof(name));
      rib_games_free(game);
      NSSavePanel *panel = [NSSavePanel savePanel];
      panel.allowedContentTypes = @[zip_type()];
      panel.nameFieldStringValue = [NSString stringWithUTF8String:name];
      panel.canCreateDirectories = YES;
      if ([panel runModal] != NSModalResponseOK || !panel.URL)
         return RIB_DATA_CANCELLED;
      if (rib_game_data_export(folders, 1, panel.URL.fileSystemRepresentation, sentence, sentence_size) != 0)
         return RIB_DATA_FAILED;
      return RIB_DATA_DONE;
   }
}

rib_data_answer rib_game_data_choose_import(char *title, size_t title_size, char *sentence, size_t sentence_size)
{
   @autoreleasepool {
      const std::string folder = data_folder();
      NSOpenPanel *panel = [NSOpenPanel openPanel];
      panel.allowedContentTypes = @[zip_type()];
      panel.canChooseFiles = YES;
      panel.canChooseDirectories = NO;
      panel.allowsMultipleSelection = NO;
      if ([panel runModal] != NSModalResponseOK || !panel.URL)
         return RIB_DATA_CANCELLED;
      chosen = panel.URL.fileSystemRepresentation;
      rib_game_t *source = rib_games_new(1);
      if (!source)
         return RIB_DATA_FAILED;
      const rib_game_data_check_t found = rib_game_data_choose(chosen.c_str(), folder.c_str(), source,
            sentence, sentence_size);
      std::snprintf(title, title_size, "%s", rib_game_get(source, "title"));
      rib_games_free(source);
      if (found == RIB_GAME_DATA_REFUSED)
      {
         chosen.clear();
         return RIB_DATA_FAILED;
      }
      return found == RIB_GAME_DATA_OTHER_GAME ? RIB_DATA_OTHER_GAME : RIB_DATA_DONE;
   }
}

rib_data_answer rib_game_data_confirm_import(char *sentence, size_t sentence_size)
{
   const std::string folder = data_folder();
   char marker[RIB_GAME_DATA_PATH_SIZE];
   if (chosen.empty())
   {
      std::snprintf(sentence, sentence_size, "Choose a zip to import first.");
      return RIB_DATA_FAILED;
   }
   if (rib_game_data_set_aside(chosen.c_str(), folder.c_str(), sentence, sentence_size) != 0)
      return RIB_DATA_FAILED;
   /* The launcher starts the game again after it closes. */
   if (fs_join(marker, sizeof(marker), folder.c_str(), RIB_DATA_RESTART_MARKER) != 0
         || fs_write_file(marker, "", 0) != 0)
   {
      std::snprintf(sentence, sentence_size, "We could not ask for the game to start again.");
      return RIB_DATA_FAILED;
   }
   chosen.clear();
   return RIB_DATA_DONE;
}
