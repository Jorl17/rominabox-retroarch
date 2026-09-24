#ifndef RIB_ACHIEVEMENTS_INTERNAL_H
#define RIB_ACHIEVEMENTS_INTERNAL_H

#include <libretro.h>
#include <stddef.h>
#include "../deps/rcheevos/include/rc_client.h"

bool rib_achievements_managed(void);
bool rib_achievements_content_load(const struct retro_game_info *info);
void rib_achievements_content_unload(void);
void rib_achievements_pump(void);
bool rib_achievements_evaluating(void);
bool rib_achievements_should_defer_restore(void);
void rib_achievements_begin_startup_gate(void);
bool rib_achievements_startup_ready(void);
bool rib_achievements_startup_gate_active(void);
void rib_achievements_finish_startup_gate(void);
void rib_achievements_event(const rc_client_event_t *event);
bool rib_achievements_badge_directory(char *path, size_t capacity);
void rib_achievements_badge_downloaded(void);
/* A badge download that ended without a picture, by the name we requested. */
void rib_achievements_badge_failed(const char *badge_name);
unsigned rib_achievements_award_request_started(void);
void rib_achievements_award_request_finished(unsigned generation);

/* A private adapter. We manage the rc_client, the hashing setup and the core
 * memory mapping only in cheevos.c. Never include this file from RmlUi. */
rc_client_t *rcheevos_rib_prepare_client(void);
rc_client_async_handle_t *rcheevos_rib_begin_identify(
      const struct retro_game_info *info, rc_client_callback_t callback,
      void *userdata);
void rcheevos_rib_complete_game_load(int result, const char *error,
      rc_client_t *client, void *userdata);

#endif
