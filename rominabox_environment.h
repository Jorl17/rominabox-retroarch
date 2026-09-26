/* The places that we pass from the ROM-in-a-Box launcher to the player in
 * the environment (ROMINABOX_*), which we always read in one way: as UTF-8 on
 * every platform.
 *
 * On Windows getenv returns text in the ANSI code page, which has no spelling
 * for every data folder name, and the libretro file layer uses UTF-8. We read
 * the wide environment and convert it to UTF-8, as for APPDATA in RetroArch
 * (file_path_special.c). On other platforms the environment already has the
 * bytes the file layer uses. */
#ifndef RIB_ENVIRONMENT_H
#define RIB_ENVIRONMENT_H

#include <stdlib.h>
#include <string.h>
#include <boolean.h>
#include <retro_inline.h>
#include <file/file_path.h>
#include "rominabox_launch.h"
#if defined(_WIN32)
#include <encodings/utf.h>
#endif

/* The variable `name` as UTF-8, allocated for the caller to free; NULL when
 * it is not set. */
static INLINE char *rib_environment(const char *name)
{
#if defined(_WIN32)
   wchar_t *wide_name = name ? utf8_to_utf16_string_alloc(name) : NULL;
   const wchar_t *wide = wide_name ? _wgetenv(wide_name) : NULL;
   char *value = wide ? utf16_to_utf8_string_alloc(wide) : NULL;
   free(wide_name);
   return value;
#else
   const char *value = name ? getenv(name) : NULL;
   size_t size = value ? strlen(value) + 1 : 0;
   char *copy = size ? (char*)malloc(size) : NULL;
   if (copy)
      memcpy(copy, value, size);
   return copy;
#endif
}

/* The game's data directory, ROMINABOX_DATA_DIR, allocated for the caller to
 * free. We accept only an absolute path, by the libretro test, so a drive
 * letter or a share counts on Windows. Anything else is NULL, never a place
 * relative to the directory the player runs in. */
static INLINE char *rib_data_directory(void)
{
   char *path = rib_environment(RIB_ENV_DATA_DIR);
   if (path && path_is_absolute(path))
      return path;
   free(path);
   return NULL;
}

#ifdef __cplusplus
#include <memory>
/* A value allocated above, for the caller to free. */
typedef std::unique_ptr<char, decltype(&free)> rib_environment_value;
static INLINE rib_environment_value rib_owned(char *value)
{
   return rib_environment_value(value, free);
}
#endif

#endif
