#ifndef RIB_MENU_FILES_H
#define RIB_MENU_FILES_H

#include <stdbool.h>
#include <file/config_file.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Failure handling differs by caller. For controls we keep a failed temporary
 * file and call rename directly. For remaps we replace on Windows and remove
 * the temporary file after a failed rename. We never free the config here. */
enum rib_config_write_policy
{
   RIB_CONFIG_WRITE_CONTROLS,
   RIB_CONFIG_WRITE_REMAP
};
bool rib_write_menu_config(config_file_t *config, const char *path,
      enum rib_config_write_policy policy);

/* Write the volume file in its exact decimal format, and clean up after a
 * close or rename failure, including replacement on Windows. */
bool rib_write_menu_volume(const char *path, float db);

#ifdef __cplusplus
}
#endif

#endif
