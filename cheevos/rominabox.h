/* The RetroAchievements interface we use on the main thread, with copies. */
#ifndef RIB_ACHIEVEMENTS_H
#define RIB_ACHIEVEMENTS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RIB_ACHIEVEMENTS_ACCOUNT_SIZE 96
#define RIB_ACHIEVEMENTS_TITLE_SIZE 128
#define RIB_ACHIEVEMENTS_DESCRIPTION_SIZE 256
#define RIB_ACHIEVEMENTS_ERROR_SIZE 256
#define RIB_ACHIEVEMENTS_BADGE_PATH_SIZE 512

typedef enum rib_achievements_status {
   RIB_ACHIEVEMENTS_EXCLUDED,
   RIB_ACHIEVEMENTS_SIGNED_OUT,
   RIB_ACHIEVEMENTS_OFF,
   RIB_ACHIEVEMENTS_SIGNING_IN,
   RIB_ACHIEVEMENTS_LOADING,
   RIB_ACHIEVEMENTS_ACTIVE,
   RIB_ACHIEVEMENTS_UNAVAILABLE,
   RIB_ACHIEVEMENTS_ERROR
} rib_achievements_status_t;

typedef enum rib_achievement_state {
   RIB_ACHIEVEMENT_LOCKED,
   RIB_ACHIEVEMENT_UNLOCKED,
   RIB_ACHIEVEMENT_UNSUPPORTED,
   RIB_ACHIEVEMENT_PENDING_UPLOAD
} rib_achievement_state_t;

/* The state of the badge picture of a row. We request a failed download
 * again the next time we show the list (rib_achievements_list_shown). */
typedef enum rib_achievement_badge {
   RIB_ACHIEVEMENT_BADGE_NONE,
   RIB_ACHIEVEMENT_BADGE_LOADING,
   RIB_ACHIEVEMENT_BADGE_READY,
   RIB_ACHIEVEMENT_BADGE_FAILED
} rib_achievement_badge_t;

typedef struct rib_achievements_snapshot {
   rib_achievements_status_t status;
   uint32_t revision;
   size_t count;
   bool enabled_preference;
   bool pending_upload;
   bool upload_failed;
   bool startup_waiting;
   bool startup_skipped;
   char account[RIB_ACHIEVEMENTS_ACCOUNT_SIZE];
   char game_title[RIB_ACHIEVEMENTS_TITLE_SIZE];
   char error[RIB_ACHIEVEMENTS_ERROR_SIZE];
} rib_achievements_snapshot_t;

typedef struct rib_achievement_row {
   uint32_t id;
   uint32_t points;
   rib_achievement_state_t state;
   rib_achievement_badge_t badge; /* READY exactly when badge_path is set */
   char title[RIB_ACHIEVEMENTS_TITLE_SIZE];
   char description[RIB_ACHIEVEMENTS_DESCRIPTION_SIZE];
   char badge_path[RIB_ACHIEVEMENTS_BADGE_PATH_SIZE];
} rib_achievement_row_t;

typedef struct rib_achievement_unlock {
   uint32_t id;
   uint32_t points;
   bool pending_upload;
   char title[RIB_ACHIEVEMENTS_TITLE_SIZE];
   char badge_path[RIB_ACHIEVEMENTS_BADGE_PATH_SIZE];
} rib_achievement_unlock_t;

/* Call every function on the main thread. Each returned struct contains
 * copies of its strings, never rc_client pointers or borrowed data. */
void rib_achievements_get_snapshot(rib_achievements_snapshot_t *out);
bool rib_achievements_get_row(size_t index, rib_achievement_row_t *out);
/* Whether the achievement list is on screen. We set it from the menu in
 * each frame we draw, and request the badges with failed downloads again
 * when the list comes back into view. */
void rib_achievements_list_shown(bool shown);
bool rib_achievements_has_unlocks(void);
bool rib_achievements_take_unlock(rib_achievement_unlock_t *out);
bool rib_achievements_has_pending_uploads(void);
bool rib_achievements_sign_in(const char *username, const char *password);
bool rib_achievements_set_enabled(bool enabled);
bool rib_achievements_retry(void);
void rib_achievements_cancel(void);
void rib_achievements_sign_out(void);
void rib_achievements_skip_startup(void);

#ifdef __cplusplus
}
#endif

#endif
