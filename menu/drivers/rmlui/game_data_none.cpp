/* DATA where we have neither a file panel nor a launcher to ask: in the
 * menu's headless tests and on other systems. */
#include "game_data_host.h"

#include <cstdio>

namespace {
rib_data_answer unavailable(char *sentence, size_t size)
{
   std::snprintf(sentence, size, "Exporting and importing data is not available here.");
   return RIB_DATA_FAILED;
}
}

rib_data_answer rib_game_data_export_to_file(char *sentence, size_t sentence_size)
{
   return unavailable(sentence, sentence_size);
}

rib_data_answer rib_game_data_choose_import(char *title, size_t title_size, char *sentence, size_t sentence_size)
{
   (void)title;
   (void)title_size;
   return unavailable(sentence, sentence_size);
}

rib_data_answer rib_game_data_confirm_import(char *sentence, size_t sentence_size)
{
   return unavailable(sentence, sentence_size);
}
