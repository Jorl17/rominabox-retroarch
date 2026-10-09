#pragma once

#include <string>

#include "events.h"
#include "menu_api.h"

namespace rib {
class Document;
class Status;

/* DATA: EXPORT DATA writes the game's data to a zip the player chooses, and
 * IMPORT DATA reads one. Before an import we ask in the screen's dialog,
 * because it replaces the game's data and restarts the game. */
class GameData
{
public:
   GameData(Document& document, Status& status) : document(document), status(status) {}
   bool handle(const Event& event);
   /* While the dialog is open, Back keeps the game's data, and OK presses
    * the focused button. */
   bool key(rib_key key);
   bool modal() const { return asking; }
   /* A screen has just been shown: we close the dialog on any other. */
   void screen_shown(bool showing);
private:
   void ask(const std::string& message, bool can_import);
   void close();
   Document& document;
   Status& status;
   bool asking = false;
};
}
