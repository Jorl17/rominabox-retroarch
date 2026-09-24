/* Per-game credential and badge paths for the managed achievements session. */
#include "rominabox_storage.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#else
#include <io.h>
#include <fcntl.h>
#include <share.h>
#include <sys/stat.h>
#endif

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
   file = fopen(path, "rb");
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

bool rib_storage_write(const rib_stored_session_t *session)
{
   char path[PATH_MAX_LENGTH];
   FILE *file;
   int descriptor;
   int length;
#ifndef _WIN32
   char temp[PATH_MAX_LENGTH];
#endif
   if (!session || !session->username[0] || !session->token[0] ||
       strchr(session->username, '\n') || strchr(session->token, '\n') ||
       !rib_storage_session_path(path))
      return false;
#ifdef _WIN32
   /* We create this directory in the launcher, with a user-only ACL. */
   if (_sopen_s(&descriptor, path, _O_WRONLY | _O_CREAT | _O_TRUNC | _O_BINARY,
         _SH_DENYRW, _S_IREAD | _S_IWRITE) != 0)
      return false;
   file = _fdopen(descriptor, "wb");
#else
   length = snprintf(temp, sizeof(temp), "%s.tmp.XXXXXX", path);
   if (length < 0 || (size_t)length >= sizeof(temp))
      return false;
   descriptor = mkstemp(temp);
   if (descriptor < 0)
      return false;
   fchmod(descriptor, 0600);
   file = fdopen(descriptor, "wb");
#endif
   if (!file)
   {
#ifdef _WIN32
      _close(descriptor);
#else
      close(descriptor);
      unlink(temp);
#endif
      return false;
   }
   length = fprintf(file, "%s\n%s\n%d\n", session->username, session->token,
         session->enabled ? 1 : 0);
   if (fclose(file) != 0 || length < 0)
   {
#ifndef _WIN32
      unlink(temp);
#endif
      return false;
   }
#ifdef _WIN32
   return true;
#else
   if (rename(temp, path) == 0)
      return true;
   unlink(temp);
   return false;
#endif
}

bool rib_storage_remove(void)
{
   char path[PATH_MAX_LENGTH];
   if (!rib_storage_session_path(path))
      return false;
   return remove(path) == 0 || errno == ENOENT;
}
