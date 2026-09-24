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

/* The step that moves a finished temporary file onto its destination. It
 * returns 0 on success, as rename does. `replaces_existing` is whether the
 * step replaces an existing destination (POSIX) or fails (Windows). In a test
 * we pass the rules of another platform here, and NULL restores this one's. */
typedef int (*rib_rename_step)(const char *from, const char *to);
void rib_files_use_rename(rib_rename_step step, bool replaces_existing);

/* Update only the device key and keep the other remap settings, with the
 * directory creation and temporary-file failure handling of the remap writer. */
bool rib_write_remap_device(const char *path, unsigned device);

#ifdef __cplusplus
}
#endif

#endif
