#include "document_contract.hpp"
#ifndef RIB_MENU_OVERLAYS_HPP
#define RIB_MENU_OVERLAYS_HPP

#include "declarations.h"
#include "document.hpp"
#include <array>
#include <cstdint>

namespace rib {
enum rib_overlay_state { RIB_OVERLAY_HIDDEN, RIB_OVERLAY_SHOWING, RIB_OVERLAY_LEAVING };
inline void paint_overlay(Document& document, const char *id, rib_overlay_state state)
{
   document.set_class(id, document_contract::Showing, state == RIB_OVERLAY_SHOWING);
   document.set_class(id, document_contract::Leaving, state == RIB_OVERLAY_LEAVING);
}

/* One timeline per menu, where each overlay follows the one declared before
 * it. On a new document we reset the entries but keep the runloop start. */
class Overlays
{
public:
   explicit Overlays(Document& document) : document(document) {}
   void load(const rib_design_data& design);
   void begin();
   void update(bool script_pending);
   bool drawing() const { return running; }
   void stop() { running = false; }
private:
   Document& document;
   struct Overlay
   {
      rib_overlay_declaration declaration{};
      int64_t started_at = 0;
      int64_t finished_at = 0;
      rib_overlay_state state = RIB_OVERLAY_HIDDEN;
      bool finished = false;
   };
   int64_t begins_at(const Overlay& overlay) const;
   std::array<Overlay, RIB_OVERLAY_MAX> overlays;
   int count = 0;
   bool running = false;
   int64_t started_at = 0;
};
}
#endif
