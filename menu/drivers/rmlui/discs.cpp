#include "discs.hpp"
#include "host.h"
#include "document.hpp"
#include "lists.hpp"
#include "document_contract.hpp"
#include "words.hpp"
#include <retro_miscellaneous.h>
#include <cstdio>
#include <string>

namespace rib {
void Discs::configure(const DesignDeclarations& design)
{
   list_id.clear();
   list_buttons.clear();
   redirect_from.clear();
   redirect_to.clear();
   synced = false;
   for (const ScreenDeclaration& screen : design.screens)
   {
      if (screen.role == ScreenRole::Discs)
      {
         list_id = screen.id;
         list_buttons = screen.buttons;
      }
      else if (!screen.images.empty())
      {
         redirect_from = screen.id;
         redirect_to = screen.images;
      }
   }
}

/* The entry exists in the document but only becomes a focus stop for a game
 * with more than one image. The list's document order is the image index. */
void Discs::sync()
{
   if (list_id.empty())
      return;
   const unsigned count = rib_host_disc_count();
   const unsigned current = rib_host_disc_index();
   if (synced && count == synced_count && current == synced_current)
      return;
   synced = true;
   synced_count = count;
   synced_current = current;
   for (const std::string& button : list_buttons)
   {
      document.set_shown(button.c_str(), count > 1);
      document.set_disabled(button.c_str(), count <= 1);
   }
   const std::string rows_id = list_id + document_contract::ListSuffix;
   const int rows = lists.rows_in(rows_id.c_str());
   for (int index = 0; index < rows; index++)
   {
      const std::string row = lists.row_in(rows_id.c_str(), index);
      if (row.empty())
         continue;
      if ((unsigned)index >= count)
      {
         document.set_shown(row.c_str(), false);
         continue;
      }
      char label[PATH_MAX_LENGTH];
      label[0] = '\0';
      rib_host_disc_label((unsigned)index, label, sizeof(label));
      const std::string title = label[0] ? std::string(label)
            : say(Word::Disc, {{"number", std::to_string(index + 1)}});
      document.set_shown(row.c_str(), true);
      lists.fit_row_title(row.c_str(), title.c_str());
   }
   /* The page shown is the one with the disc in the drive. */
   const std::string current_row = count > 0 && current < (unsigned)rows
         ? lists.row_in(rows_id.c_str(), (int)current) : std::string();
   if (rows > 0)
      lists.retarget_pages(rows_id.c_str(), current_row.empty() ? nullptr : current_row.c_str());
   if (!current_row.empty())
      lists.select_row(rows_id.c_str(), current_row.c_str(), say(Word::DiscMark).c_str(), "");
   const std::string status = rows > 0 && count > (unsigned)rows
         ? say(Word::ShowingDiscs, {{"shown", std::to_string(rows)}, {"count", std::to_string(count)}})
         : std::string();
   document.set_element_text((list_id + document_contract::StatusSuffix).c_str(), status.c_str());
}

bool Discs::choose(const char *id)
{
   if (!id || list_id.empty())
      return false;
   const std::string rows_id = list_id + document_contract::ListSuffix;
   const int rows = lists.rows_in(rows_id.c_str());
   const unsigned count = rib_host_disc_count();
   for (int index = 0; index < rows; index++)
   {
      if (lists.row_in(rows_id.c_str(), index) != id)
         continue;
      if ((unsigned)index >= count)
         return true;
      rib_host_choose_disc((unsigned)index);
      sync();
      return true;
   }
   return false;
}

std::string Discs::redirect(const std::string& screen) const
{
   if (!redirect_from.empty() && screen == redirect_from
         && rib_host_disc_count() > 1 && !redirect_to.empty())
      return redirect_to;
   return screen;
}
}
