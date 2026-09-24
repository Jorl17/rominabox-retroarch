#include "overlays.hpp"
#include "host.h"
#include <cstring>

namespace rib {
void Overlays::load(const rib_design_data& design)
{
   if (notification_active()) paint_notification();
   count = design.overlay_count;
   for (int index = 0; index < count; ++index)
   {
      overlays[index] = Overlay{};
      overlays[index].declaration = design.overlays[index];
   }
}

void Overlays::begin()
{
   // We read the declarations in the first frame we request. With an empty
   // design we stop requesting frames in that frame, after the script step.
   running = true;
   started_at = 0;
   rib_host_overlay_frames(true);
}

int64_t Overlays::begins_at(const Overlay& overlay) const
{
   if (!*overlay.declaration.follows)
      return started_at;
   for (int index = 0; index < count; ++index)
   {
      const auto& before = overlays[index];
      if (&before == &overlay)
         break;
      if (std::strcmp(before.declaration.id, overlay.declaration.follows) == 0)
         return before.finished ? before.finished_at : 0;
   }
   return started_at;
}

void Overlays::update(bool script_pending)
{
   const int64_t now = rib_host_time_us();
   if (running && !started_at)
      started_at = now;
   bool pending = false;
   for (int index = 0; running && index < count; ++index)
   {
      auto& overlay = overlays[index];
      const auto& declaration = overlay.declaration;
      if (overlay.finished)
         continue;
      if (!overlay.started_at)
      {
         overlay.started_at = begins_at(overlay);
         if (!overlay.started_at)
         {
            pending = true;
            continue;
         }
      }
      const int elapsed = (int)((now - overlay.started_at) / 1000);
      rib_overlay_state want = RIB_OVERLAY_HIDDEN;
      if (elapsed >= declaration.after_ms + declaration.hold_ms + declaration.leave_ms)
      {
         overlay.finished = true;
         overlay.finished_at = now;
      }
      else if (elapsed >= declaration.after_ms + declaration.hold_ms)
         want = RIB_OVERLAY_LEAVING;
      else if (elapsed >= declaration.after_ms)
         want = RIB_OVERLAY_SHOWING;
      if (want != overlay.state)
      {
         overlay.state = want;
         paint_overlay(document, declaration.id, want);
      }
      if (!overlay.finished)
         pending = true;
   }
   running = pending;
   if (notification_until && now >= notification_until)
      clear_notification();
   if (!drawing() && !script_pending)
      rib_host_overlay_frames(false);
}
}

namespace rib {
void Overlays::paint_notification()
{
   document.set_element_text(document_contract::UnlockTitle, notification.title.c_str());
   document.set_element_text(document_contract::UnlockDetail, notification.detail.c_str());
   if (auto *badge = document.root() ? document.root()->GetElementById(document_contract::UnlockBadge) : nullptr) {
      if (!notification.badge.empty()) badge->SetAttribute("src", notification.badge);
      document.set_shown(document_contract::UnlockBadge, !notification.badge.empty());
   }
   document.set_shown(document_contract::UnlockRow, true);
}
void Overlays::notify(const Notification& next)
{
   // We show one readable notification at a time, from a queue in this feature.
   constexpr int64_t hold_us = 4500000;
   notification = next;
   notification_until = rib_host_time_us() + hold_us;
   paint_notification();
   rib_host_overlay_frames(true);
}
void Overlays::clear_notification()
{
   notification_until = 0;
   notification = {};
   document.set_shown(document_contract::UnlockRow, false);
}
}
