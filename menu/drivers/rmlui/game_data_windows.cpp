/* DATA on Windows: the game's sandbox lets it reach no file the player
 * chooses, so the launcher shows the file dialog and reads or writes the zip,
 * outside the sandbox, and answers through the controller relay. */
#include "game_data_host.h"

#include "../../../input/drivers/rominabox_dinput.h"
#include "../../../rominabox_pad_relay.h"

rib_data_answer rib_game_data_export_to_file(char *sentence, size_t sentence_size)
{
   char title[256];
   return rib_pad_relay_game_data(RIB_PAD_RELAY_EXPORT_DATA, title, sizeof(title), sentence, sentence_size);
}

rib_data_answer rib_game_data_choose_import(char *title, size_t title_size, char *sentence, size_t sentence_size)
{
   return rib_pad_relay_game_data(RIB_PAD_RELAY_CHOOSE_IMPORT, title, title_size, sentence, sentence_size);
}

rib_data_answer rib_game_data_confirm_import(char *sentence, size_t sentence_size)
{
   char title[256];
   return rib_pad_relay_game_data(RIB_PAD_RELAY_CONFIRM_IMPORT, title, sizeof(title), sentence, sentence_size);
}
