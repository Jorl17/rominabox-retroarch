#ifndef RIB_ACHIEVEMENTS_CATALOG_H
#define RIB_ACHIEVEMENTS_CATALOG_H

#include "rominabox.h"
#include "../deps/rcheevos/include/rc_client.h"

/* A main-thread copy of the game data in the one rc_client, which is part of
 * the session. In event callbacks, only mark it dirty or queue copied unlocks. */
bool rib_catalog_initialize(void);
void rib_catalog_clear(rib_achievements_snapshot_t *snapshot);
void rib_catalog_mark_rows_dirty(void);
void rib_catalog_badge_downloaded(void);
void rib_catalog_triggered(const rc_client_achievement_t *achievement);
void rib_catalog_pump(rc_client_t *client, rib_achievements_snapshot_t *snapshot);
bool rib_catalog_get_row(rc_client_t *client, size_t index,
      rib_achievement_row_t *out);
bool rib_catalog_take_unlock(rib_achievement_unlock_t *out,
      bool pending_upload);
bool rib_catalog_has_unlocks(void);

#endif
