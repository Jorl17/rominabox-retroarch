#ifndef RIB_ACHIEVEMENTS_STORAGE_H
#define RIB_ACHIEVEMENTS_STORAGE_H

#include <stdbool.h>
#include <stddef.h>
#include "rominabox.h"

typedef struct rib_stored_session {
   char username[RIB_ACHIEVEMENTS_ACCOUNT_SIZE];
   char token[128];
   bool enabled;
} rib_stored_session_t;

/* All paths are inside the absolute per-game data directory we were given. */
bool rib_storage_available(void);
bool rib_storage_badge_directory(char *path, size_t capacity);
bool rib_storage_badge_name_valid(const char *name);
bool rib_storage_read(rib_stored_session_t *session);
bool rib_storage_write(const rib_stored_session_t *session);
bool rib_storage_remove(void);

#endif
