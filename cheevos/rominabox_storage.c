/* Per-game credential and badge paths for the managed achievements session. */
#include "rominabox_storage.h"
#include "portable_fs.h" /* ROM-in-a-Box's file layer, shared with the launcher */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <file/file_path.h>
#include <retro_miscellaneous.h>

static const char *rib_data_directory(void)
{
   const char *path = getenv("ROMINABOX_DATA_DIR");
   if (!path || !*path || !path_is_absolute(path) || !path_is_directory(path))
      return NULL;
   return path;
}

bool rib_storage_available(void)
{
   return rib_data_directory() != NULL;
}

static bool rib_storage_session_path(char path[PATH_MAX_LENGTH])
{
   const char *directory = rib_data_directory();
   int length;
   if (!directory)
      return false;
   length = snprintf(path, PATH_MAX_LENGTH, "%s/achievements.session", directory);
   return length > 0 && length < PATH_MAX_LENGTH;
}

bool rib_storage_badge_directory(char *path, size_t capacity)
{
   const char *directory = rib_data_directory();
   int length;
   if (!directory || !path || !capacity)
      return false;
   length = snprintf(path, capacity, "%s/achievements-badges", directory);
   return length > 0 && (size_t)length < capacity;
}

bool rib_storage_badge_name_valid(const char *name)
{
   const char *p;
   if (!name || !*name)
      return false;
   for (p = name; *p; ++p)
      if (!((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'z') ||
            (*p >= 'A' && *p <= 'Z') || *p == '_' || *p == '-'))
         return false;
   return true;
}

bool rib_storage_read(rib_stored_session_t *session)
{
   char path[PATH_MAX_LENGTH];
   char enabled[8];
   FILE *file;
   size_t size;
   if (!session || !rib_storage_session_path(path))
      return false;
   memset(session, 0, sizeof(*session));
   file = fs_open(path, "rb");
   if (!file)
      return false;
   if (!fgets(session->username, sizeof(session->username), file) ||
       !fgets(session->token, sizeof(session->token), file) ||
       !fgets(enabled, sizeof(enabled), file))
      goto invalid;
   size = strlen(session->username);
   if (!size || session->username[size - 1] != '\n')
      goto invalid;
   session->username[size - 1] = '\0';
   size = strlen(session->token);
   if (!size || session->token[size - 1] != '\n')
      goto invalid;
   session->token[size - 1] = '\0';
   if (!session->username[0] || !session->token[0] ||
       (strcmp(enabled, "0\n") != 0 && strcmp(enabled, "1\n") != 0))
      goto invalid;
   session->enabled = enabled[0] == '1';
   fclose(file);
   return true;

invalid:
   memset(session, 0, sizeof(*session));
   fclose(file);
   return false;
}

/* We replace the whole file and make it readable by this user only. */
bool rib_storage_write(const rib_stored_session_t *session)
{
   char path[PATH_MAX_LENGTH];
   char text[sizeof(session->username) + sizeof(session->token) + 8];
   int length;
   bool written;
   if (!session || !session->username[0] || !session->token[0] ||
       strchr(session->username, '\n') || strchr(session->token, '\n') ||
       !rib_storage_session_path(path))
      return false;
   length = snprintf(text, sizeof(text), "%s\n%s\n%d\n", session->username,
         session->token, session->enabled ? 1 : 0);
   if (length <= 0 || (size_t)length >= sizeof(text))
      return false;
   written = fs_write_file(path, text, (size_t)length) == 0;
   memset(text, 0, sizeof(text));
   return written;
}

bool rib_storage_remove(void)
{
   char path[PATH_MAX_LENGTH];
   if (!rib_storage_session_path(path))
      return false;
   return fs_remove(path) == 0;
}
