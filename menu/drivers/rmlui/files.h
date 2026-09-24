#ifndef RIB_MENU_FILES_H
#define RIB_MENU_FILES_H

#include <stdbool.h>
#include <file/config_file.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Write through a temporary file, then replace the file at path with it. The
 * config still belongs to the caller. */
bool rib_write_menu_config(config_file_t *config, const char *path);

/* The volume file's exact decimal format, replaced the same way. */
bool rib_write_menu_volume(const char *path, float db);

/* The step that moves a finished temporary file onto its destination and
 * replaces an existing file in one step. It returns 0 on success, as rename
 * does. After a failed step the destination is as it was. In a test we pass
 * another step here, and NULL restores the step for this platform. */
typedef int (*rib_rename_step)(const char *from, const char *to);
void rib_files_use_rename(rib_rename_step step);

/* Update only the device key and keep the other remap settings, with the
 * directory creation and temporary-file failure handling of the remap writer. */
bool rib_write_remap_device(const char *path, unsigned device);

#ifdef __cplusplus
}
#endif

#endif
