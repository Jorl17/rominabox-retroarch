#ifndef RIB_MENU_DISCS_HPP
#define RIB_MENU_DISCS_HPP

#include "declarations.h"
#include <cstddef>

namespace rib {
class Document;
class Lists;

/* The disc list metadata is from the design screens, in declaration order.
 * We pass in the active screen and the redirect target id from Menu. */
class Discs
{
public:
   Discs(Document& document, Lists& lists) : document(document), lists(lists) {}
   void configure(const rib_design_data& design);
   void sync() const;
   bool choose(const char *screen, const char *id) const;
   void redirect(char *screen_id, size_t length) const;
private:
   Document& document;
   Lists& lists;
   char list_id[32]{};
   char list_button[32]{};
   char mark[32]{};
   char redirect_from[32]{};
   char redirect_to[32]{};
};
}
#endif
