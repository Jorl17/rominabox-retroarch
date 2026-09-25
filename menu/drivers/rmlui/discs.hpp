#ifndef RIB_MENU_DISCS_HPP
#define RIB_MENU_DISCS_HPP

#include "declarations.h"
#include "list_owner.hpp"
#include <string>

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
   void configure(const DesignDeclarations& design);
   void sync();
   ScreenRole role() const override { return ScreenRole::Discs; }
   /* Put the disc of the row in the tray. */
   bool choose(const char *row) override;
   /* The screen to show for `screen`: the one in its declaration when the
    * core has loaded several images, or else `screen` itself. */
   std::string redirect(const std::string& screen) const;
private:
   Document& document;
   Lists& lists;
   /* The disc list's screen, the buttons that open it and its tray word. */
   std::string list_id;
   std::vector<std::string> list_buttons;
   /* A screen in the design that opens the disc list instead. */
   std::string redirect_from, redirect_to;
   /* What we last filled the list from. The answer from the core changes only
    * when a disc is swapped, and filling the list again resets the pages. */
   unsigned synced_count = 0, synced_current = 0;
   bool synced = false;
};
}
#endif
