#ifndef RIB_MENU_OVERLAYS_HPP
#define RIB_MENU_OVERLAYS_HPP

#include "declarations.h"
#include "document.hpp"
#include "document_contract.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace rib {
enum rib_overlay_state { RIB_OVERLAY_HIDDEN, RIB_OVERLAY_SHOWING, RIB_OVERLAY_LEAVING };
inline void paint_overlay(Document& document, const std::string& id, rib_overlay_state state)
{
   document.set_class(id.c_str(), document_contract::Showing, state == RIB_OVERLAY_SHOWING);
   document.set_class(id.c_str(), document_contract::Leaving, state == RIB_OVERLAY_LEAVING);
}

/* One timeline per menu, where each overlay follows the one declared before
 * it. On a new document we reset the entries but keep the runloop start. */
class Overlays
{
public:
   explicit Overlays(Document& document) : document(document) {}
   void load(const DesignDeclarations& design);
   void begin();
   void update(bool script_pending);
   struct Notification { std::string title, detail, badge; };
   void notify(const Notification& notification);
   /* The badge of the notification on screen, once its picture exists. */
   void show_badge(const std::string& badge);
   void clear_notification();
   bool notification_active() const { return notification_until != 0; }
   bool drawing() const { return running || notification_active(); }
   /* Whether the game must wait, because an overlay that delays it is still
    * on screen, or we have not yet read from the design which ones do. */
   bool holding_game() const;
   void stop() { running = false; clear_notification(); }
private:
   Document& document;
   struct Overlay
   {
      OverlayDeclaration declaration;
      int64_t started_at = 0;
      int64_t finished_at = 0;
      rib_overlay_state state = RIB_OVERLAY_HIDDEN;
      bool finished = false;
   };
   int64_t begins_at(const Overlay& overlay) const;
   std::vector<Overlay> overlays;
   void paint_notification();
   Notification notification;
   int64_t notification_until = 0;
   bool running = false;
   bool loaded = false;
   int64_t started_at = 0;
};
}
#endif
