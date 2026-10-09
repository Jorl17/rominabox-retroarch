#include "game_data.hpp"

#include <cctype>

#include <RmlUi/Core.h>

#include "document.hpp"
#include "document_contract.hpp"
#include "game_data_host.h"
#include "host.h"
#include "sounds.hpp"
#include "status.hpp"
#include "words.hpp"
#include "../../../verbosity.h"

namespace rib {
namespace {
/* The menu's words are in capitals in every design, and not every design's
 * font has curly quotes, so we write straight ones. */
std::string capitals(const std::string& text)
{
   std::string written;
   for (size_t at = 0; at < text.size(); at++)
   {
      /* “ and ” in UTF-8. */
      if (text.compare(at, 3, "\xe2\x80\x9c") == 0 || text.compare(at, 3, "\xe2\x80\x9d") == 0)
      {
         written += '"';
         at += 2;
      }
      else
         written += (char)std::toupper((unsigned char)text[at]);
   }
   return written;
}
}

bool GameData::handle(const Event& event)
{
   char sentence[1024] = "";
   char title[256] = "";
   switch (event.kind)
   {
      case RIB_RMLUI_ACTION_DATA_EXPORT:
         if (asking)
            return true;
         play_action_sound(event.kind);
         switch (rib_game_data_export_to_file(sentence, sizeof(sentence)))
         {
            case RIB_DATA_DONE:
               status.set_data(say(Word::DataExported).c_str());
               break;
            case RIB_DATA_CANCELLED:
               break;
            default:
               RARCH_ERR("[RIB] could not export the game's data: %s\n", sentence);
               status.set_data(say(Word::DataExportFailed).c_str());
               break;
         }
         return true;
      case RIB_RMLUI_ACTION_DATA_IMPORT:
         if (asking)
            return true;
         play_action_sound(event.kind);
         switch (rib_game_data_choose_import(title, sizeof(title), sentence, sizeof(sentence)))
         {
            case RIB_DATA_DONE:
               ask(say(Word::DataImportSame), true);
               break;
            case RIB_DATA_OTHER_GAME:
               ask(say(Word::DataImportOther, {{"game", capitals(title)}}), true);
               break;
            case RIB_DATA_FAILED:
               RARCH_LOG("[RIB] the chosen zip cannot be imported: %s\n", sentence);
               ask(capitals(sentence), false);
               break;
            default:
               break;
         }
         return true;
      case RIB_RMLUI_ACTION_DATA_IMPORT_CONFIRM:
         if (!asking)
            return true;
         play_action_sound(event.kind);
         close();
         if (rib_game_data_confirm_import(sentence, sizeof(sentence)) == RIB_DATA_DONE)
            rib_host_quit();
         else
         {
            RARCH_ERR("[RIB] could not keep the chosen zip for the next start: %s\n", sentence);
            status.set_data(say(Word::DataImportFailed).c_str());
         }
         return true;
      case RIB_RMLUI_ACTION_DATA_IMPORT_CANCEL:
         close();
         return true;
      default:
         /* While we ask, only the dialog's buttons act. */
         return asking;
   }
}

bool GameData::key(rib_key key)
{
   if (!asking)
      return false;
   if (key == RIB_KEY_CANCEL || key == RIB_KEY_TOGGLE || key == RIB_KEY_RESUME)
   {
      close();
      return true;
   }
   if (key == RIB_KEY_OK || key == RIB_KEY_SELECT)
   {
      if (Rml::Element *focused = document.get_context()->GetFocusElement())
         focused->Click();
      return true;
   }
   return key == RIB_KEY_START;
}

void GameData::screen_shown(bool showing)
{
   if (!showing && asking)
      close();
}

void GameData::ask(const std::string& message, bool can_import)
{
   Rml::Element *root = document.root();
   if (!root)
      return;
   if (Rml::Element *words = root->GetElementById(document_contract::DataImportMessage))
      words->SetInnerRML(Rml::StringUtilities::EncodeRml(message));
   if (Rml::Element *confirm = root->GetElementById(document_contract::DataImportConfirm))
   {
      if (can_import)
         confirm->RemoveAttribute("disabled");
      else
         confirm->SetAttribute("disabled", "");
      confirm->SetClass(document_contract::Disabled, !can_import);
   }
   document.set_shown(document_contract::DataImportDialog, true);
   asking = true;
   /* We start on the button that keeps the game's data. */
   if (Rml::Element *keep = root->GetElementById(document_contract::DataImportCancel))
      keep->Focus(true);
}

void GameData::close()
{
   document.set_shown(document_contract::DataImportDialog, false);
   asking = false;
   if (Rml::Element *root = document.root())
      if (Rml::Element *import = root->GetElementById(document_contract::DataImport))
         import->Focus(true);
}
}
