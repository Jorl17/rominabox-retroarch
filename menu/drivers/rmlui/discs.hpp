#ifndef RIB_MENU_DISCS_HPP
#define RIB_MENU_DISCS_HPP

#include "declarations.h"
#include "list_owner.hpp"
#include <cstddef>

namespace rib {
class Document;
class Lists;

/* The disc list metadata is from the design screens, in declaration order.
 * We pass in the active screen and the redirect target id from Menu. We may
 * call sync() every frame, but we fill the list again only when the disc
 * count or current disc changes, or after configure() for a new document. */
class Discs : public ListOwner
{
public:
   Discs(Document& document, Lists& lists) : document(document), lists(lists) {}
   void configure(const rib_design_data& design);
   void sync();
   const char *role() const override { return "discs"; }
   /* Put the disc of the row in the tray. */
   bool choose(const char *row) override;
   void redirect(char *screen_id, size_t length) const;
private:
   Document& document;
   Lists& lists;
   char list_id[32]{};
   char list_button[32]{};
   char mark[32]{};
   char redirect_from[32]{};
   char redirect_to[32]{};
   /* What we last filled the list from. The answer from the core changes only
    * when a disc is swapped, and filling the list again resets the pages. */
   unsigned synced_count = 0, synced_current = 0;
   bool synced = false;
};
}
#endif
