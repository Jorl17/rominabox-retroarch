/* Empty achievements functions for player builds without HAVE_CHEEVOS. */
#include "rominabox.h"
#include <string.h>

void rib_achievements_get_snapshot(rib_achievements_snapshot_t *out)
{
   if (out)
   {
      memset(out, 0, sizeof(*out));
      out->status = RIB_ACHIEVEMENTS_EXCLUDED;
   }
}

bool rib_achievements_get_row(size_t index, rib_achievement_row_t *out)
{
   (void)index;
   (void)out;
   return false;
}

bool rib_achievements_has_unlocks(void) { return false; }
bool rib_achievements_take_unlock(rib_achievement_unlock_t *out)
{
   (void)out;
   return false;
}
bool rib_achievements_has_pending_uploads(void) { return false; }
bool rib_achievements_sign_in(const char *username, const char *password)
{
   (void)username;
   (void)password;
   return false;
}
bool rib_achievements_set_enabled(bool enabled)
{
   (void)enabled;
   return false;
}
bool rib_achievements_retry(void) { return false; }
void rib_achievements_cancel(void) {}
void rib_achievements_sign_out(void) {}
void rib_achievements_skip_startup(void) {}
