/* The Casual achievements session for ROM-in-a-Box. We use the evaluator,
 * HTTP transport, game hashing and core memory map of RetroArch/rcheevos. */
#include "rominabox.h"
#include "rominabox_internal.h"
#include "rominabox_storage.h"
#include "cheevos.h"
#include "cheevos_client.h"
#include "cheevos_locals.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <compat/strl.h>
#include <file/file_path.h>
#include <retro_miscellaneous.h>
#include "../configuration.h"

typedef struct rib_unlock_node {
   rib_achievement_unlock_t value;
   struct rib_unlock_node *next;
} rib_unlock_node_t;

typedef enum rib_completion_kind {
   RIB_COMPLETION_NONE,
   RIB_COMPLETION_LOGIN,
   RIB_COMPLETION_LOAD
} rib_completion_kind_t;

typedef enum rib_upload_event {
   RIB_UPLOAD_EVENT_NONE,
   RIB_UPLOAD_EVENT_DISCONNECTED,
   RIB_UPLOAD_EVENT_RECONNECTED
} rib_upload_event_t;

typedef struct rib_completion {
   rib_completion_kind_t kind;
   int result;
   char error[RIB_ACHIEVEMENTS_ERROR_SIZE];
} rib_completion_t;

typedef struct rib_session {
   rib_achievements_snapshot_t snapshot;
   rib_achievement_row_t *rows;
   bool *badge_requested;
   char username[RIB_ACHIEVEMENTS_ACCOUNT_SIZE];
   char token[128];
   char content_path[PATH_MAX_LENGTH];
   uint8_t *content_data;
   size_t content_size;
   unsigned generation;
   bool content_present;
   bool rows_dirty;
   bool badge_dirty;
   bool unlock_changed;
   rib_upload_event_t pending_upload_event;
   unsigned inflight_awards;
   bool retry_pending;
   bool upload_error_changed;
   char upload_error[RIB_ACHIEVEMENTS_ERROR_SIZE];
   rib_completion_t completion;
   rib_unlock_node_t *unlock_head;
   rib_unlock_node_t *unlock_tail;
#ifdef HAVE_THREADS
   slock_t *lock;
#endif
} rib_session_t;

static rib_session_t rib;

static void rib_lock(void)
{
#ifdef HAVE_THREADS
   if (rib.lock)
      slock_lock(rib.lock);
#endif
}

static void rib_unlock(void)
{
#ifdef HAVE_THREADS
   if (rib.lock)
      slock_unlock(rib.lock);
#endif
}

static void rib_copy(char *dest, const char *src, size_t capacity)
{
   strlcpy(dest, src ? src : "", capacity);
}

static bool rib_included(void)
{
   const char *flag = getenv("ROMINABOX_ACHIEVEMENTS");
   return flag && strcmp(flag, "1") == 0;
}

bool rib_achievements_managed(void)
{
   return getenv("ROMINABOX_ACHIEVEMENTS") != NULL;
}

bool rib_achievements_badge_directory(char *path, size_t capacity)
{
   return rib_included() && rib_storage_badge_directory(path, capacity);
}

static void rib_clear_unlocks(void)
{
   rib_unlock_node_t *node;
   rib_lock();
   node = rib.unlock_head;
   rib.unlock_head = rib.unlock_tail = NULL;
   rib_unlock();
   while (node)
   {
      rib_unlock_node_t *next = node->next;
      free(node);
      node = next;
   }
}

static void rib_clear_rows(void)
{
   free(rib.rows);
   free(rib.badge_requested);
   rib.rows = NULL;
   rib.badge_requested = NULL;
   rib.snapshot.count = 0;
   rib.snapshot.revision++;
}

static void rib_error(const char *message)
{
   rib_copy(rib.snapshot.error, message, sizeof(rib.snapshot.error));
   rib.snapshot.status = RIB_ACHIEVEMENTS_ERROR;
   rib.snapshot.revision++;
}

static void rib_read_session(void)
{
   rib_stored_session_t stored;
   if (!rib_storage_read(&stored))
      return;
   rib_copy(rib.username, stored.username, sizeof(rib.username));
   rib_copy(rib.token, stored.token, sizeof(rib.token));
   rib.snapshot.enabled_preference = stored.enabled;
   rib_copy(rib.snapshot.account, rib.username, sizeof(rib.snapshot.account));
}

static bool rib_write_session(void)
{
   rib_stored_session_t stored = {{0}};
   if (!rib.token[0])
      return rib_storage_remove();
   rib_copy(stored.username, rib.username, sizeof(stored.username));
   rib_copy(stored.token, rib.token, sizeof(stored.token));
   stored.enabled = rib.snapshot.enabled_preference;
   return rib_storage_write(&stored);
}

static void rib_badge_path(char path[RIB_ACHIEVEMENTS_BADGE_PATH_SIZE],
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

static rib_achievement_state_t rib_row_state(
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

static void rib_refresh_rows(void)
{
   rc_client_achievement_list_t *list;
   rc_client_t *client = get_rcheevos_locals()->client;
   rib_achievement_row_t *rows;
   bool *badge_requested;
   size_t count = 0, index = 0;
   uint32_t bucket, item;
   const rc_client_game_t *game;

   if (!client || !rc_client_is_game_loaded(client))
   {
      rib_clear_rows();
      return;
   }
   game = rc_client_get_game_info(client);
   rib_copy(rib.snapshot.game_title, game ? game->title : "",
         sizeof(rib.snapshot.game_title));
   list = rc_client_create_achievement_list(client,
         RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE,
         RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_LOCK_STATE);
   if (!list)
      return;

   for (bucket = 0; bucket < list->num_buckets; ++bucket)
      count += list->buckets[bucket].num_achievements;
   rows = count ? (rib_achievement_row_t*)calloc(count, sizeof(*rows)) : NULL;
   badge_requested = count ? (bool*)calloc(count, sizeof(*badge_requested)) : NULL;
   if (count && (!rows || !badge_requested))
   {
      free(rows);
      free(badge_requested);
      rc_client_destroy_achievement_list(list);
      return;
   }

   for (bucket = 0; bucket < list->num_buckets; ++bucket)
   {
      const rc_client_achievement_bucket_t *group = &list->buckets[bucket];
      for (item = 0; item < group->num_achievements; ++item)
      {
         const rc_client_achievement_t *achievement = group->achievements[item];
         rib_achievement_row_t *row = &rows[index++];
         row->id = achievement->id;
         row->points = achievement->points;
         row->state = rib_row_state(achievement);
         rib_copy(row->title, achievement->title, sizeof(row->title));
         rib_copy(row->description, achievement->description,
               sizeof(row->description));
         rib_badge_path(row->badge_path, achievement);
      }
   }
   rc_client_destroy_achievement_list(list);
   free(rib.rows);
   free(rib.badge_requested);
   rib.rows = rows;
   rib.badge_requested = badge_requested;
   rib.snapshot.count = count;
   rib.snapshot.revision++;
}

static void rib_refresh_badge_paths(void)
{
   rc_client_t *client = get_rcheevos_locals()->client;
   size_t index;
   if (!client || !rc_client_is_game_loaded(client))
      return;
   for (index = 0; index < rib.snapshot.count; ++index)
   {
      const rc_client_achievement_t *achievement;
      char path[RIB_ACHIEVEMENTS_BADGE_PATH_SIZE];
      if (rib.rows[index].badge_path[0])
         continue;
      achievement = rc_client_get_achievement_info(client, rib.rows[index].id);
      rib_badge_path(path, achievement);
      if (path[0])
      {
         rib_copy(rib.rows[index].badge_path, path,
               sizeof(rib.rows[index].badge_path));
         rib.snapshot.revision++;
      }
   }
}

/* An HTTP task can complete off the main thread. We change the copied rows
 * and publish a new revision only in the next pump on the main thread. */
void rib_achievements_badge_downloaded(void)
{
   rib_lock();
   rib.badge_dirty = true;
   rib_unlock();
}

unsigned rib_achievements_award_request_started(void)
{
   unsigned generation = 0;
   rib_lock();
   if (rib.content_present)
   {
      generation = rib.generation;
      ++rib.inflight_awards;
   }
   rib_unlock();
   return generation;
}

void rib_achievements_award_request_finished(unsigned generation)
{
   rib_lock();
   if (generation == rib.generation && rib.inflight_awards)
      --rib.inflight_awards;
   rib_unlock();
}

static void rib_queue_completion(rib_completion_kind_t kind, int result,
      const char *error, void *userdata)
{
   unsigned generation = (unsigned)(uintptr_t)userdata;
   rib_lock();
   if (generation == rib.generation)
   {
      rib.completion.kind = kind;
      rib.completion.result = result;
      rib_copy(rib.completion.error, error, sizeof(rib.completion.error));
   }
   rib_unlock();
}

static void rib_login_callback(int result, const char *error,
      rc_client_t *client, void *userdata)
{
   (void)client;
   rib_queue_completion(RIB_COMPLETION_LOGIN, result, error, userdata);
}

static void rib_load_callback(int result, const char *error,
      rc_client_t *client, void *userdata)
{
   (void)client;
   rib_queue_completion(RIB_COMPLETION_LOAD, result, error, userdata);
}

static bool rib_begin_login(bool token_login, const char *password)
{
   rc_client_t *client;
   unsigned generation;
   char transient_password[256];

   if (!rib.content_present || !rib.username[0])
      return false;
   client = rcheevos_rib_prepare_client();
   if (!client)
   {
      rib_error("Cannot initialize achievements.");
      return false;
   }
   rc_client_set_hardcore_enabled(client, 0);
   rc_client_set_unofficial_enabled(client, 0);
   rc_client_set_encore_mode_enabled(client, 0);
   rc_client_set_spectator_mode_enabled(client, 0);

   rib_lock();
   generation = ++rib.generation;
   rib.completion.kind = RIB_COMPLETION_NONE;
   rib_unlock();

   rib.snapshot.status = RIB_ACHIEVEMENTS_SIGNING_IN;
   rib.snapshot.error[0] = '\0';
   rib.snapshot.revision++;

   if (token_login)
      rc_client_begin_login_with_token(client, rib.username, rib.token,
            rib_login_callback, (void*)(uintptr_t)generation);
   else
   {
      rib_copy(transient_password, password, sizeof(transient_password));
      rc_client_begin_login_with_password(client, rib.username,
            transient_password, rib_login_callback,
            (void*)(uintptr_t)generation);
      memset(transient_password, 0, sizeof(transient_password));
   }
   return true;
}

void rib_achievements_pump(void)
{
   rib_completion_t completion;
   rc_client_t *client;
   const rc_client_user_t *user;
   struct retro_game_info info;
   unsigned generation;
   rib_upload_event_t pending_upload_event;
   unsigned inflight_awards;
   bool unlock_changed;
   bool badge_dirty;
   bool upload_error_changed;
   char upload_error[RIB_ACHIEVEMENTS_ERROR_SIZE];

   if (!rib_achievements_managed())
      return;
   rib_lock();
   completion = rib.completion;
   rib.completion.kind = RIB_COMPLETION_NONE;
   generation = rib.generation;
   pending_upload_event = rib.pending_upload_event;
   rib.pending_upload_event = RIB_UPLOAD_EVENT_NONE;
   inflight_awards = rib.inflight_awards;
   unlock_changed = rib.unlock_changed;
   rib.unlock_changed = false;
   badge_dirty = rib.badge_dirty;
   rib.badge_dirty = false;
   upload_error_changed = rib.upload_error_changed;
   rib.upload_error_changed = false;
   rib_copy(upload_error, rib.upload_error, sizeof(upload_error));
   rib_unlock();

   if (pending_upload_event == RIB_UPLOAD_EVENT_DISCONNECTED)
      rib.retry_pending = true;
   else if (pending_upload_event == RIB_UPLOAD_EVENT_RECONNECTED)
      rib.retry_pending = false;
   if (rib.snapshot.pending_upload !=
       (rib.retry_pending || inflight_awards > 0))
   {
      rib.snapshot.pending_upload = rib.retry_pending || inflight_awards > 0;
      rib.snapshot.revision++;
   }
   if (upload_error_changed)
   {
      rib.snapshot.upload_failed = true;
      rib_copy(rib.snapshot.error, upload_error, sizeof(rib.snapshot.error));
      rib.snapshot.revision++;
   }
   if (unlock_changed)
      rib.snapshot.revision++;

   client = get_rcheevos_locals()->client;
   if (completion.kind == RIB_COMPLETION_LOGIN &&
       rib.snapshot.status == RIB_ACHIEVEMENTS_SIGNING_IN)
   {
      if (completion.result != RC_OK)
      {
         if (completion.result == RC_EXPIRED_TOKEN ||
             completion.result == RC_INVALID_CREDENTIALS)
         {
            rib.token[0] = '\0';
            rib.snapshot.enabled_preference = false;
            rib.snapshot.status = RIB_ACHIEVEMENTS_SIGNED_OUT;
            rib_copy(rib.snapshot.error, "Sign in again to enable achievements.",
                  sizeof(rib.snapshot.error));
            if (!rib_write_session())
               rib_error("Cannot remove the expired achievements session from game storage.");
         }
         else
            rib_error(completion.error[0] ? completion.error : "Sign in failed.");
         rib.snapshot.revision++;
      }
      else
      {
         user = rc_client_get_user_info(client);
         if (!user || !user->token || !user->token[0])
            rib_error("RetroAchievements did not return an account token.");
         else
         {
            rib_copy(rib.username, user->username, sizeof(rib.username));
            rib_copy(rib.token, user->token, sizeof(rib.token));
            rib_copy(rib.snapshot.account, user->display_name,
                  sizeof(rib.snapshot.account));
            rib.snapshot.enabled_preference = true;
            if (!rib_write_session())
               rib_error("Cannot save the achievements session in game storage.");
            else
            {
               info.path = rib.content_path;
               info.data = rib.content_data;
               info.size = rib.content_size;
               info.meta = NULL;
               rib.snapshot.status = RIB_ACHIEVEMENTS_LOADING;
               rib.snapshot.revision++;
               rcheevos_rib_begin_identify(&info, rib_load_callback,
                     (void*)(uintptr_t)generation);
            }
         }
      }
   }
   else if (completion.kind == RIB_COMPLETION_LOAD &&
            rib.snapshot.status == RIB_ACHIEVEMENTS_LOADING)
   {
      if (completion.result == RC_OK)
      {
         rcheevos_rib_complete_game_load(completion.result,
               completion.error, client, (void*)(uintptr_t)generation);
         if (get_rcheevos_locals()->core_supports &&
             rc_client_is_game_loaded(client))
         {
            rib.snapshot.status = RIB_ACHIEVEMENTS_ACTIVE;
            rib.rows_dirty = true;
         }
         else
         {
            rib.snapshot.status = RIB_ACHIEVEMENTS_UNAVAILABLE;
            rib_copy(rib.snapshot.error,
                  "This core does not expose achievement memory.",
                  sizeof(rib.snapshot.error));
         }
      }
      else if (completion.result == RC_NO_GAME_LOADED)
      {
         rib.snapshot.status = RIB_ACHIEVEMENTS_UNAVAILABLE;
         rib_copy(rib.snapshot.error, "No achievements available for this game.",
               sizeof(rib.snapshot.error));
      }
      else
         rib_error(completion.error[0] ? completion.error :
               "Could not load achievements for this game.");
      rib.snapshot.revision++;
   }

   rib_lock();
   if (rib.rows_dirty)
   {
      rib.rows_dirty = false;
      rib_unlock();
      rib_refresh_rows();
   }
   else
      rib_unlock();
   if (badge_dirty)
      rib_refresh_badge_paths();
}

bool rib_achievements_evaluating(void)
{
   return rib.snapshot.status == RIB_ACHIEVEMENTS_ACTIVE;
}

bool rib_achievements_should_defer_restore(void)
{
   return rib_included() && rib.content_present &&
         rib.snapshot.enabled_preference &&
         rib.snapshot.status != RIB_ACHIEVEMENTS_ACTIVE;
}

void rib_achievements_begin_startup_gate(void)
{
   rib.snapshot.startup_waiting = true;
   rib.snapshot.startup_skipped = false;
   rib.snapshot.revision++;
}

bool rib_achievements_startup_ready(void)
{
   return rib.snapshot.startup_skipped ||
         rib.snapshot.status == RIB_ACHIEVEMENTS_ACTIVE;
}

bool rib_achievements_startup_gate_active(void)
{
   return rib.snapshot.startup_waiting;
}

void rib_achievements_finish_startup_gate(void)
{
   rib.snapshot.startup_waiting = false;
   rib.snapshot.startup_skipped = false;
   rib.snapshot.revision++;
}

void rib_achievements_skip_startup(void)
{
   if (!rib.snapshot.startup_waiting)
      return;
   rib_achievements_set_enabled(false);
   rib.snapshot.startup_skipped = true;
   rib.snapshot.revision++;
}

void rib_achievements_event(const rc_client_event_t *event)
{
   rib_unlock_node_t *node;
   if (!event || !rib_achievements_managed())
      return;
   if (event->type == RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED &&
       event->achievement && rib_achievements_evaluating())
   {
      node = (rib_unlock_node_t*)calloc(1, sizeof(*node));
      if (node)
      {
         node->value.id = event->achievement->id;
         node->value.points = event->achievement->points;
         rib_copy(node->value.title, event->achievement->title,
               sizeof(node->value.title));
         rib_badge_path(node->value.badge_path, event->achievement);
         rib_lock();
         if (rib.content_present &&
             rib.snapshot.status == RIB_ACHIEVEMENTS_ACTIVE)
         {
            if (rib.unlock_tail)
               rib.unlock_tail->next = node;
            else
               rib.unlock_head = node;
            rib.unlock_tail = node;
            node = NULL;
            rib.rows_dirty = true;
            rib.unlock_changed = true;
         }
         rib_unlock();
         free(node);
      }
   }
   else if (event->type == RC_CLIENT_EVENT_DISCONNECTED)
   {
      rib_lock();
      rib.pending_upload_event = RIB_UPLOAD_EVENT_DISCONNECTED;
      rib.rows_dirty = true;
      rib_unlock();
   }
   else if (event->type == RC_CLIENT_EVENT_RECONNECTED)
   {
      rib_lock();
      rib.pending_upload_event = RIB_UPLOAD_EVENT_RECONNECTED;
      rib.rows_dirty = true;
      rib_unlock();
   }
   else if (event->type == RC_CLIENT_EVENT_SERVER_ERROR &&
            event->server_error && event->server_error->api &&
            strcmp(event->server_error->api, "award_achievement") == 0)
   {
      rib_lock();
      rib_copy(rib.upload_error,
            event->server_error->error_message ?
            event->server_error->error_message :
            "An achievement upload was rejected by RetroAchievements.",
            sizeof(rib.upload_error));
      rib.upload_error_changed = true;
      rib.rows_dirty = true;
      rib_unlock();
   }
}

bool rib_achievements_content_load(const struct retro_game_info *info)
{
   settings_t *settings = config_get_ptr();
   rib_achievements_content_unload();
   memset(&rib.snapshot, 0, sizeof(rib.snapshot));
   rib.snapshot.status = RIB_ACHIEVEMENTS_EXCLUDED;
   if (settings)
   {
      settings->bools.cheevos_enable = rib_included();
      settings->bools.cheevos_hardcore_mode_enable = false;
   }
   if (!rib_included())
      return false;
   if (!rib_storage_available())
   {
      rib_error("The game storage path must be an existing absolute directory.");
      return false;
   }

#ifdef HAVE_THREADS
   if (!rib.lock)
      rib.lock = slock_new();
   if (!rib.lock)
   {
      rib_error("Cannot initialize achievements.");
      return false;
   }
#endif
   if (!info || (!info->path && !info->data))
   {
      rib_error("No game content is available for achievements.");
      return false;
   }
   if (info->path)
      rib_copy(rib.content_path, info->path, sizeof(rib.content_path));
   if (info->data && info->size)
   {
      rib.content_data = (uint8_t*)malloc(info->size);
      if (!rib.content_data)
      {
         rib_error("Cannot keep game content for achievement identification.");
         return false;
      }
      memcpy(rib.content_data, info->data, info->size);
      rib.content_size = info->size;
   }
   rib.content_present = true;
   rib_read_session();
   rib.snapshot.status = rib.token[0] ? RIB_ACHIEVEMENTS_OFF :
         RIB_ACHIEVEMENTS_SIGNED_OUT;
   if (rib.snapshot.enabled_preference)
      rib_begin_login(true, NULL);
   return true;
}

void rib_achievements_content_unload(void)
{
   rc_client_t *client = get_rcheevos_locals()->client;
   rib_lock();
   ++rib.generation;
   rib.completion.kind = RIB_COMPLETION_NONE;
   rib.content_present = false;
   rib.pending_upload_event = RIB_UPLOAD_EVENT_NONE;
   rib.inflight_awards = 0;
   rib.retry_pending = false;
   rib.upload_error_changed = false;
   rib.upload_error[0] = '\0';
   rib.badge_dirty = false;
   rib.unlock_changed = false;
   rib_unlock();
   if (client)
      rc_client_unload_game(client);
   rib_clear_unlocks();
   rib_clear_rows();
   free(rib.content_data);
   rib.content_data = NULL;
   rib.content_size = 0;
   rib.content_path[0] = '\0';
   rib.snapshot.pending_upload = false;
   rib.snapshot.upload_failed = false;
   rib.snapshot.game_title[0] = '\0';
   rib.snapshot.startup_waiting = false;
   rib.snapshot.startup_skipped = false;
}

void rib_achievements_get_snapshot(rib_achievements_snapshot_t *out)
{
   if (!out)
      return;
   rib_achievements_pump();
   *out = rib.snapshot;
   if (!rib_included())
      out->status = RIB_ACHIEVEMENTS_EXCLUDED;
}

bool rib_achievements_get_row(size_t index, rib_achievement_row_t *out)
{
   if (!out || !rib_included())
      return false;
   rib_achievements_pump();
   if (index >= rib.snapshot.count)
      return false;
   *out = rib.rows[index];
   if (!out->badge_path[0])
   {
      const rc_client_achievement_t *achievement = rc_client_get_achievement_info(
            get_rcheevos_locals()->client, out->id);
      if (achievement && !rib.badge_requested[index] &&
          rib_storage_badge_name_valid(achievement->badge_name))
      {
         const bool unlocked = out->state == RIB_ACHIEVEMENT_UNLOCKED ||
               out->state == RIB_ACHIEVEMENT_PENDING_UPLOAD;
         char badge_name[32];
         int length;
         if (unlocked)
            length = snprintf(badge_name, sizeof(badge_name), "%s",
                  achievement->badge_name);
         else
            length = snprintf(badge_name, sizeof(badge_name), "%s_lock",
                  achievement->badge_name);
         if (length > 0 && length < sizeof(badge_name) &&
             rib_storage_badge_name_valid(badge_name) &&
             rcheevos_client_download_badge_from_url(unlocked ?
                   achievement->badge_url : achievement->badge_locked_url,
                   badge_name))
            rib.badge_requested[index] = true;
      }
   }
   return true;
}

bool rib_achievements_take_unlock(rib_achievement_unlock_t *out)
{
   rib_unlock_node_t *node;
   if (!out)
      return false;
   rib_lock();
   node = rib.unlock_head;
   if (node)
   {
      rib.unlock_head = node->next;
      if (!rib.unlock_head)
         rib.unlock_tail = NULL;
   }
   rib_unlock();
   if (!node)
      return false;
   *out = node->value;
   out->pending_upload = rib.snapshot.pending_upload;
   free(node);
   return true;
}

bool rib_achievements_has_unlocks(void)
{
   bool result;
   rib_lock();
   result = rib.unlock_head != NULL;
   rib_unlock();
   return result;
}

bool rib_achievements_has_pending_uploads(void)
{
   rib_achievements_pump();
   return rib.snapshot.pending_upload;
}

bool rib_achievements_sign_in(const char *username, const char *password)
{
   if (!rib_included() || !rib.content_present || !username || !password ||
       !*username || !*password || strchr(username, '\n') ||
       strlen(username) >= sizeof(rib.username) ||
       strlen(password) >= 256 ||
       rib.token[0] ||
       rib.snapshot.status == RIB_ACHIEVEMENTS_SIGNING_IN ||
       rib.snapshot.status == RIB_ACHIEVEMENTS_LOADING)
      return false;
   rib_achievements_cancel();
   rib_copy(rib.username, username, sizeof(rib.username));
   rib.token[0] = '\0';
   return rib_begin_login(false, password);
}

bool rib_achievements_set_enabled(bool enabled)
{
   rc_client_t *client;
   if (!rib_included() || !rib.content_present)
      return false;
   if (!enabled)
   {
      rib.snapshot.enabled_preference = false;
      if (rib.snapshot.status == RIB_ACHIEVEMENTS_SIGNING_IN ||
          rib.snapshot.status == RIB_ACHIEVEMENTS_LOADING)
         rib_achievements_cancel();
      rib.snapshot.status = rib.token[0] ? RIB_ACHIEVEMENTS_OFF :
            RIB_ACHIEVEMENTS_SIGNED_OUT;
      rib.snapshot.revision++;
      if (rib.token[0] && !rib_write_session())
      {
         rib_error("Cannot save the achievements setting in game storage.");
         return false;
      }
      return true;
   }
   if (!rib.token[0])
      return false;
   if (rib.snapshot.status == RIB_ACHIEVEMENTS_ACTIVE)
      return true;
   client = get_rcheevos_locals()->client;
   rib.snapshot.enabled_preference = true;
   if (client && rc_client_get_user_info(client) &&
       rc_client_is_game_loaded(client))
   {
      /* We evaluated no frames while OFF, so discard partial hit counts from
       * before then. Earned and unsent awards stay after rc_client_reset. */
      rc_client_reset(client);
      rib.snapshot.status = RIB_ACHIEVEMENTS_ACTIVE;
      rib.snapshot.revision++;
      if (!rib_write_session())
      {
         rib_error("Cannot save the achievements setting in game storage.");
         return false;
      }
      return true;
   }
   return rib_begin_login(true, NULL);
}

bool rib_achievements_retry(void)
{
   if (!rib.token[0] || !rib.content_present ||
       (rib.snapshot.status != RIB_ACHIEVEMENTS_ERROR &&
        rib.snapshot.status != RIB_ACHIEVEMENTS_UNAVAILABLE))
      return false;
   return rib_begin_login(true, NULL);
}

void rib_achievements_cancel(void)
{
   rc_client_t *client = get_rcheevos_locals()->client;
   bool had_pending_upload;
   rib_lock();
   had_pending_upload = rib.inflight_awards > 0 || rib.retry_pending;
   ++rib.generation;
   rib.completion.kind = RIB_COMPLETION_NONE;
   rib.inflight_awards = 0;
   rib.retry_pending = false;
   rib.pending_upload_event = RIB_UPLOAD_EVENT_NONE;
   rib_unlock();
   if (client && (rib.snapshot.status == RIB_ACHIEVEMENTS_SIGNING_IN ||
                  rib.snapshot.status == RIB_ACHIEVEMENTS_LOADING))
      rc_client_logout(client);
   rib.snapshot.status = rib.token[0] ? RIB_ACHIEVEMENTS_OFF :
         RIB_ACHIEVEMENTS_SIGNED_OUT;
   rib.snapshot.enabled_preference = false;
   rib.snapshot.pending_upload = false;
   if (had_pending_upload)
   {
      rib.snapshot.upload_failed = true;
      rib_copy(rib.snapshot.error,
            "An achievement upload was not confirmed before signing out.",
            sizeof(rib.snapshot.error));
   }
   rib.snapshot.revision++;
   if (rib.token[0] && !rib_write_session())
      rib_error("Cannot save the achievements setting in game storage.");
}

void rib_achievements_sign_out(void)
{
   rc_client_t *client = get_rcheevos_locals()->client;
   rib_achievements_cancel();
   if (client)
      rc_client_logout(client);
   rib_clear_unlocks();
   rib_clear_rows();
   rib.username[0] = rib.token[0] = '\0';
   rib.snapshot.account[0] = rib.snapshot.game_title[0] = '\0';
   rib.snapshot.pending_upload = false;
   rib.snapshot.status = RIB_ACHIEVEMENTS_SIGNED_OUT;
   rib.snapshot.revision++;
   if (!rib_storage_remove())
      rib_error("Cannot remove the achievements session from game storage.");
}
