#include "discs.hpp"
#include "host.h"
#include "document.hpp"
#include "lists.hpp"
#include "parts.hpp"
#include "slots.hpp"
#include "../../../audio/volume_range.h"
#include <retro_miscellaneous.h>
#include <string/stdstring.h>
#include <cstdio>

namespace rib {
void Discs::configure(const rib_design_data& design)
{
   list_id[0] = '\0';
   list_button[0] = '\0';
   mark[0] = '\0';
   redirect_from[0] = '\0';
   redirect_to[0] = '\0';
   for (size_t index = 0; index < design.screen_count; ++index)
   {
      const rib_screen_declaration *screen = &design.screens[index];
      if (string_is_equal(screen->images, "list"))
      {
         strlcpy(list_id, screen->id, sizeof(list_id));
         strlcpy(list_button, screen->button, sizeof(list_button));
         strlcpy(mark, screen->mark, sizeof(mark));
      }
      else if (screen->images[0])
      {
         strlcpy(redirect_from, screen->id, sizeof(redirect_from));
         strlcpy(redirect_to, screen->images, sizeof(redirect_to));
      }
   }
}

/* The entry exists in the document but only becomes a focus stop for a game
 * with more than one image. The list's document order is the image index. */
void Discs::sync() const
{
   char rows_id[48];
   unsigned count;
   unsigned current;
   int rows;
   int index;

   if (!list_id[0])
      return;
   count = rib_host_disc_count();
   if (list_button[0])
   {
      document.set_shown(list_button, count > 1);
      document.set_disabled(list_button, count <= 1);
   }
   snprintf(rows_id, sizeof(rows_id), "%s-list", list_id);
   rows = lists.rows_in(rows_id);
   current = rib_host_disc_index();
   for (index = 0; index < rows; index++)
   {
      const char *row = lists.row_in(rows_id, index);
      char label[PATH_MAX_LENGTH];

      if (!row)
         continue;
      if ((unsigned)index >= count)
      {
         document.set_shown(row, false);
         continue;
      }
      label[0] = '\0';
      rib_host_disc_label((unsigned)index, label, sizeof(label));
      if (!label[0])
         snprintf(label, sizeof(label), "Disc %u", (unsigned)index + 1);
      document.set_shown(row, true);
      lists.fit_row_title(row, label);
   }
   if (rows > 0)
      lists.retarget_pages(rows_id);
   if (count > 0 && current < (unsigned)rows)
   {
      const char *row = lists.row_in(rows_id, (int)current);

      if (row)
         lists.select_row(rows_id, row, mark, "");
   }
   {
      char status_id[40];
      char status[64];

      snprintf(status_id, sizeof(status_id), "%s-status", list_id);
      status[0] = '\0';
      if (rows > 0 && count > (unsigned)rows)
         snprintf(status, sizeof(status), "SHOWING %d OF %u", rows, count);
      document.set_element_text(status_id, status);
   }
}

bool Discs::choose(const char *screen, const char *id) const
{
   char rows_id[48];
   unsigned count;
   int rows;
   int index;

   if (!screen || !id || !list_id[0])
      return false;
   if (!string_is_equal(screen, list_id))
      return false;
   snprintf(rows_id, sizeof(rows_id), "%s-list", list_id);
   rows = lists.rows_in(rows_id);
   count = rib_host_disc_count();
   for (index = 0; index < rows; index++)
   {
      const char *row = lists.row_in(rows_id, index);
      unsigned image;

      if (!row || !string_is_equal(row, id))
         continue;
      if ((unsigned)index >= count)
         return true;
      image = (unsigned)index;
      rib_host_choose_disc(image);
      return true;
   }
   return false;
}

void Discs::redirect(char *screen_id, size_t length) const
{
   if (screen_id && screen_id[0] && redirect_from[0]
         && string_is_equal(screen_id, redirect_from)
         && rib_host_disc_count() > 1 && redirect_to[0])
      strlcpy(screen_id, redirect_to, length);
}
}
