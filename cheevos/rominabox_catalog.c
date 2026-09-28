/* Copied achievement rows, badge paths and new-unlock notifications. */
#include "rominabox_catalog.h"
#include "rominabox_internal.h"
#include "rominabox_storage.h"
#include "cheevos_client.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <compat/strl.h>
#include <features/features_cpu.h>
#include <file/file_path.h>
#include <retro_miscellaneous.h>
#ifdef HAVE_THREADS
#include <rthreads/rthreads.h>
#endif

typedef struct rib_unlock_node {
   rib_achievement_unlock_t value;
   struct rib_unlock_node *next;
} rib_unlock_node_t;

typedef struct rib_badge_failure {
   char name[32];
   struct rib_badge_failure *next;
} rib_badge_failure_t;

/* We show a badge not on disk as on its way. We request it, and request it
 * again this long after a refused request or a failed download. */
#define RIB_BADGE_RETRY_USEC (5 * 1000000)

typedef struct rib_catalog {
   rib_achievement_row_t *rows;
   /* For each row, when to request its badge again, or 0. */
   int64_t *retry_at;
   size_t count;
   bool rows_dirty;
   bool badge_dirty;
   bool unlock_changed;
   bool list_shown;
   rib_unlock_node_t *unlock_head;
   rib_unlock_node_t *unlock_tail;
   rib_badge_failure_t *failures;
#ifdef HAVE_THREADS
   slock_t *lock;
#endif
} rib_catalog_t;

static rib_catalog_t catalog;

/* RetroAchievements has some warnings in the form of achievements, which
 * unlock as soon as a game loads, for example "Unsupported Game Version"
 * for a version with no achievements. Their ids start here. In rcheevos
 * versions newer than the copy in deps/, they are not in the summaries
 * (rc_client.c, RC_CLIENT_ACHIEVEMENT_WARNING_ID). We never list, count or
 * show them as unlocked, because they are not the player's achievements. */
#define CATALOG_WARNING_ID 101000001u

static bool catalog_is_warning(const rc_client_achievement_t *achievement)
{
   return achievement->id >= CATALOG_WARNING_ID;
}

static void catalog_lock(void)
{
#ifdef HAVE_THREADS
   if (catalog.lock)
      slock_lock(catalog.lock);
#endif
}

static void catalog_unlock(void)
{
#ifdef HAVE_THREADS
   if (catalog.lock)
      slock_unlock(catalog.lock);
#endif
}

bool rib_catalog_initialize(void)
{
#ifdef HAVE_THREADS
   if (!catalog.lock)
      catalog.lock = slock_new();
   return catalog.lock != NULL;
#else
   return true;
#endif
}

static void catalog_free_failures(rib_badge_failure_t *failure)
{
   while (failure)
   {
      rib_badge_failure_t *next = failure->next;
      free(failure);
      failure = next;
   }
}

void rib_catalog_clear(rib_achievements_snapshot_t *snapshot)
{
   rib_unlock_node_t *node;
   rib_badge_failure_t *failures;
   catalog_lock();
   node = catalog.unlock_head;
   catalog.unlock_head = catalog.unlock_tail = NULL;
   failures = catalog.failures;
   catalog.failures = NULL;
   catalog.rows_dirty = false;
   catalog.badge_dirty = false;
   catalog.unlock_changed = false;
   catalog_unlock();
   catalog_free_failures(failures);
   while (node)
   {
      rib_unlock_node_t *next = node->next;
      free(node);
      node = next;
   }
   free(catalog.rows);
   free(catalog.retry_at);
   catalog.rows = NULL;
   catalog.retry_at = NULL;
   catalog.count = 0;
   snapshot->count = 0;
   snapshot->game_title[0] = '\0';
   snapshot->revision++;
}

void rib_catalog_mark_rows_dirty(void)
{
   catalog_lock();
   catalog.rows_dirty = true;
   catalog_unlock();
}

/* An HTTP task can complete off the main thread. */
void rib_catalog_badge_downloaded(void)
{
   catalog_lock();
   catalog.badge_dirty = true;
   catalog_unlock();
}

/* Also off the main thread. We queue it and match rows in the next pump. */
void rib_catalog_badge_failed(const char *badge_name)
{
   rib_badge_failure_t *failure;
   if (!badge_name || !*badge_name)
      return;
   failure = (rib_badge_failure_t*)calloc(1, sizeof(*failure));
   if (!failure)
      return;
   strlcpy(failure->name, badge_name, sizeof(failure->name));
   catalog_lock();
   failure->next = catalog.failures;
   catalog.failures = failure;
   catalog_unlock();
}

static void catalog_badge_path(char path[RIB_ACHIEVEMENTS_BADGE_PATH_SIZE],
      const rc_client_achievement_t *achievement)
{
   char directory[PATH_MAX_LENGTH];
   int length;
   path[0] = '\0';
   if (!achievement ||
       !rib_storage_badge_name_valid(achievement->badge_name) ||
       !rib_achievements_badge_directory(directory, sizeof(directory)))
      return;
   length = snprintf(path, RIB_ACHIEVEMENTS_BADGE_PATH_SIZE,
         "%s/%s%s.png", directory, achievement->badge_name,
         (achievement->unlocked & RC_CLIENT_ACHIEVEMENT_UNLOCKED_SOFTCORE) ?
         "" : "_lock");
   if (length < 0 || length >= RIB_ACHIEVEMENTS_BADGE_PATH_SIZE ||
       !path_is_valid(path))
      path[0] = '\0';
}

static rib_achievement_state_t catalog_row_state(
      const rc_client_achievement_t *achievement)
{
   if (achievement->bucket == RC_CLIENT_ACHIEVEMENT_BUCKET_UNSYNCED)
      return RIB_ACHIEVEMENT_PENDING_UPLOAD;
   if (achievement->state == RC_CLIENT_ACHIEVEMENT_STATE_DISABLED)
      return RIB_ACHIEVEMENT_UNSUPPORTED;
   if (achievement->unlocked & RC_CLIENT_ACHIEVEMENT_UNLOCKED_SOFTCORE)
      return RIB_ACHIEVEMENT_UNLOCKED;
   return RIB_ACHIEVEMENT_LOCKED;
}

static bool catalog_unlocked(const rib_achievement_row_t *row)
{
   return row->state == RIB_ACHIEVEMENT_UNLOCKED ||
         row->state == RIB_ACHIEVEMENT_PENDING_UPLOAD;
}

/* The name under which we download a row's badge. It is the unlocked
 * picture once the player earns the achievement, and the locked one before. */
static bool catalog_badge_name(char name[32],
      const rc_client_achievement_t *achievement, const rib_achievement_row_t *row)
{
   int length;
   if (!achievement || !rib_storage_badge_name_valid(achievement->badge_name))
      return false;
   length = snprintf(name, 32, "%s%s", achievement->badge_name,
         catalog_unlocked(row) ? "" : "_lock");
   return length > 0 && length < 32 && rib_storage_badge_name_valid(name);
}

/* Request a row's picture, and show it as on its way until it is on disk.
 * We repeat a request after a pause when it was refused because RetroArch
 * is already fetching the same file, or when we could not queue it. */
static void catalog_request_badge(rib_achievement_row_t *row, int64_t *retry_at,
      const rc_client_achievement_t *achievement)
{
   char name[32];
   if (!catalog_badge_name(name, achievement, row))
      return;
   *retry_at = 0;
   if (rcheevos_client_download_badge_from_url(catalog_unlocked(row) ?
            achievement->badge_url : achievement->badge_locked_url, name))
   {
      row->badge = RIB_ACHIEVEMENT_BADGE_LOADING;
      return;
   }
   catalog_badge_path(row->badge_path, achievement);
   if (row->badge_path[0])
   {
      row->badge = RIB_ACHIEVEMENT_BADGE_READY;
      return;
   }
   row->badge = RIB_ACHIEVEMENT_BADGE_LOADING;
   *retry_at = cpu_features_get_time_usec() + RIB_BADGE_RETRY_USEC;
}

static void catalog_refresh_rows(rc_client_t *client,
      rib_achievements_snapshot_t *snapshot)
{
   rc_client_achievement_list_t *list;
   rib_achievement_row_t *rows;
   int64_t *retry_at;
   size_t count = 0, index = 0, previous;
   uint32_t bucket, item;
   const rc_client_game_t *game;

   if (!client || !rc_client_is_game_loaded(client))
   {
      rib_catalog_clear(snapshot);
      return;
   }
   game = rc_client_get_game_info(client);
   strlcpy(snapshot->game_title, game && game->title ? game->title : "",
         sizeof(snapshot->game_title));
   list = rc_client_create_achievement_list(client,
         RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE,
         RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_LOCK_STATE);
   if (!list)
      return;

   for (bucket = 0; bucket < list->num_buckets; ++bucket)
      for (item = 0; item < list->buckets[bucket].num_achievements; ++item)
         if (!catalog_is_warning(list->buckets[bucket].achievements[item]))
            count++;
   rows = count ? (rib_achievement_row_t*)calloc(count, sizeof(*rows)) : NULL;
   retry_at = count ? (int64_t*)calloc(count, sizeof(*retry_at)) : NULL;
   if (count && (!rows || !retry_at))
   {
      free(rows);
      free(retry_at);
      rc_client_destroy_achievement_list(list);
      return;
   }

   for (bucket = 0; bucket < list->num_buckets; ++bucket)
   {
      const rc_client_achievement_bucket_t *group = &list->buckets[bucket];
      for (item = 0; item < group->num_achievements; ++item)
      {
         const rc_client_achievement_t *achievement = group->achievements[item];
         int64_t *row_retry;
         rib_achievement_row_t *row;
         if (catalog_is_warning(achievement))
            continue;
         row_retry = &retry_at[index];
         row = &rows[index++];
         row->id = achievement->id;
         row->points = achievement->points;
         row->state = catalog_row_state(achievement);
         strlcpy(row->title, achievement->title ? achievement->title : "",
               sizeof(row->title));
         strlcpy(row->description,
               achievement->description ? achievement->description : "",
               sizeof(row->description));
         catalog_badge_path(row->badge_path, achievement);
         row->badge = row->badge_path[0] ?
               RIB_ACHIEVEMENT_BADGE_READY : RIB_ACHIEVEMENT_BADGE_NONE;
         if (row->badge_path[0])
            continue;
         /* We keep a download in progress or failed as it is across a
          * refresh, or we would request each missing badge again on unlock.
          * The player has just earned a row whose picture changed, so we
          * request its colour badge now, for the popup. */
         for (previous = 0; previous < catalog.count; ++previous)
            if (catalog.rows[previous].id == row->id)
            {
               if (catalog_unlocked(&catalog.rows[previous]) == catalog_unlocked(row))
               {
                  row->badge = catalog.rows[previous].badge;
                  *row_retry = catalog.retry_at[previous];
               }
               else
                  catalog_request_badge(row, row_retry, achievement);
               break;
            }
      }
   }
   rc_client_destroy_achievement_list(list);
   free(catalog.rows);
   free(catalog.retry_at);
   catalog.rows = rows;
   catalog.retry_at = retry_at;
   catalog.count = snapshot->count = count;
   snapshot->revision++;
}

static void catalog_refresh_badge_paths(rc_client_t *client,
      rib_achievements_snapshot_t *snapshot)
{
   size_t index;
   if (!client || !rc_client_is_game_loaded(client))
      return;
   for (index = 0; index < catalog.count; ++index)
   {
      const rc_client_achievement_t *achievement;
      char path[RIB_ACHIEVEMENTS_BADGE_PATH_SIZE];
      if (catalog.rows[index].badge_path[0])
         continue;
      achievement = rc_client_get_achievement_info(client, catalog.rows[index].id);
      catalog_badge_path(path, achievement);
      if (path[0])
      {
         strlcpy(catalog.rows[index].badge_path, path,
               sizeof(catalog.rows[index].badge_path));
         catalog.rows[index].badge = RIB_ACHIEVEMENT_BADGE_READY;
         snapshot->revision++;
      }
   }
}

/* We still show a failed download as on its way, and request it again
 * after a pause. */
static void catalog_mark_failures(rc_client_t *client,
      rib_badge_failure_t *failures)
{
   size_t index;
   for (index = 0; client && failures && index < catalog.count; ++index)
   {
      rib_achievement_row_t *row = &catalog.rows[index];
      const rib_badge_failure_t *failure;
      char name[32];
      if (row->badge != RIB_ACHIEVEMENT_BADGE_LOADING ||
          !catalog_badge_name(name,
               rc_client_get_achievement_info(client, row->id), row))
         continue;
      for (failure = failures; failure; failure = failure->next)
         if (!strcmp(failure->name, name))
         {
            catalog.retry_at[index] = cpu_features_get_time_usec() + RIB_BADGE_RETRY_USEC;
            break;
         }
   }
   catalog_free_failures(failures);
}

/* Each time we show the list, we request again at once every badge not on
 * disk, without waiting for a reply that may never come or for a retry. */
void rib_catalog_list_shown(bool shown, rib_achievements_snapshot_t *snapshot)
{
   size_t index;
   bool retry = shown && !catalog.list_shown;
   catalog.list_shown = shown;
   for (index = 0; retry && index < catalog.count; ++index)
      if (catalog.rows[index].badge == RIB_ACHIEVEMENT_BADGE_LOADING)
      {
         catalog.rows[index].badge = RIB_ACHIEVEMENT_BADGE_NONE;
         catalog.retry_at[index] = 0;
         snapshot->revision++;
      }
}

/* Request again every badge whose pause is over. */
static void catalog_retry_badges(rc_client_t *client)
{
   size_t index;
   const int64_t now = cpu_features_get_time_usec();
   for (index = 0; client && index < catalog.count; ++index)
      if (catalog.retry_at[index] && now >= catalog.retry_at[index])
         catalog_request_badge(&catalog.rows[index], &catalog.retry_at[index],
               rc_client_get_achievement_info(client, catalog.rows[index].id));
}

void rib_catalog_pump(rc_client_t *client, rib_achievements_snapshot_t *snapshot)
{
   bool rows_dirty, badge_dirty, unlock_changed;
   rib_badge_failure_t *failures;
   catalog_lock();
   rows_dirty = catalog.rows_dirty;
   badge_dirty = catalog.badge_dirty;
   unlock_changed = catalog.unlock_changed;
   failures = catalog.failures;
   catalog.failures = NULL;
   catalog.rows_dirty = catalog.badge_dirty = catalog.unlock_changed = false;
   catalog_unlock();
   if (rows_dirty)
      catalog_refresh_rows(client, snapshot);
   if (badge_dirty)
      catalog_refresh_badge_paths(client, snapshot);
   catalog_mark_failures(client, failures);
   catalog_retry_badges(client);
   if (unlock_changed)
      snapshot->revision++;
}

void rib_catalog_triggered(const rc_client_achievement_t *achievement)
{
   rib_unlock_node_t *node;
   if (!achievement || catalog_is_warning(achievement))
      return;
   node = (rib_unlock_node_t*)calloc(1, sizeof(*node));
   if (!node)
      return;
   node->value.id = achievement->id;
   node->value.points = achievement->points;
   strlcpy(node->value.title,
         achievement->title ? achievement->title : "",
         sizeof(node->value.title));
   catalog_badge_path(node->value.badge_path, achievement);
   catalog_lock();
   if (catalog.unlock_tail)
      catalog.unlock_tail->next = node;
   else
      catalog.unlock_head = node;
   catalog.unlock_tail = node;
   catalog.rows_dirty = true;
   catalog.unlock_changed = true;
   catalog_unlock();
}

bool rib_catalog_get_row(rc_client_t *client, size_t index,
      rib_achievement_row_t *out)
{
   rib_achievement_row_t *row;
   if (index >= catalog.count || !out)
      return false;
   row = &catalog.rows[index];
   if (row->badge == RIB_ACHIEVEMENT_BADGE_NONE && client)
      catalog_request_badge(row, &catalog.retry_at[index],
            rc_client_get_achievement_info(client, row->id));
   *out = *row;
   return true;
}

bool rib_catalog_take_unlock(rib_achievement_unlock_t *out,
      bool pending_upload)
{
   rib_unlock_node_t *node;
   if (!out)
      return false;
   catalog_lock();
   node = catalog.unlock_head;
   if (node)
   {
      catalog.unlock_head = node->next;
      if (!catalog.unlock_head)
         catalog.unlock_tail = NULL;
   }
   catalog_unlock();
   if (!node)
      return false;
   *out = node->value;
   out->pending_upload = pending_upload;
   free(node);
   return true;
}

bool rib_catalog_has_unlocks(void)
{
   bool result;
   catalog_lock();
   result = catalog.unlock_head != NULL;
   catalog_unlock();
   return result;
}
