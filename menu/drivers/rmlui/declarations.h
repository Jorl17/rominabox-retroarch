#ifndef RIB_MENU_DECLARATIONS_H
#define RIB_MENU_DECLARATIONS_H

#include <stdbool.h>
#include <stddef.h>
#include <retro_miscellaneous.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The exported format's fixed bounds. */
enum { RIB_OVERLAY_MAX = 8, RIB_TOGGLE_MAX = 8 };
enum { RIB_SHADER_MAX = 32, RIB_CONTROL_MAX = 48, RIB_DEVICE_MAX = 8 };
enum rib_toggle_guard { RIB_TOGGLE_GUARD_NONE, RIB_TOGGLE_GUARD_SAVES };

typedef struct rib_toggle
{
   char id[64];
   char on[32];
   char off[32];
   char guard_label[64];
   char guard_status[128];
   enum rib_toggle_guard guard;
   bool state;
} rib_toggle_t;

typedef struct rib_screen_declaration
{
   char id[512];
   char panel[128];
   char heading[128];
   char footer[128];
   char button[128];
   char images[64];
   char mark[32];
} rib_screen_declaration;

typedef struct rib_overlay_declaration
{
   char id[64];
   char follows[64];
   char needs[128];
   int after_ms;
   int hold_ms;
   int leave_ms;
} rib_overlay_declaration;

typedef struct rib_design_data
{
   const rib_screen_declaration *screens;
   size_t screen_count;
   rib_overlay_declaration overlays[RIB_OVERLAY_MAX];
   int overlay_count;
   rib_toggle_t toggles[RIB_TOGGLE_MAX];
   int toggle_count;
   char binds_list[64];
   int binds_after_ms;
   int binds_width;
} rib_design_data;

/* One exported shader id and its preset. We keep the order of the shader
 * file and the fixed bounds of the format, and we include empty presets. */
typedef struct rib_shader_declaration
{
   char id[64];
   char preset[PATH_MAX_LENGTH];
} rib_shader_declaration;

typedef struct rib_shader_catalog
{
   rib_shader_declaration entries[RIB_SHADER_MAX];
   int count;
   char state_on[32];
   char state_off[32];
} rib_shader_catalog;

/* Controls and controller variants in the exported declaration order. We
 * borrow this catalog in the view and never call into the menu for items. */
typedef struct rib_control_declaration
{
   char id[32];
   char group[32];
   unsigned bind_index;
   bool enabled;
   char label[NAME_MAX_LENGTH];
} rib_control_declaration;

typedef struct rib_device_declaration
{
   char id[32];
   char name[NAME_MAX_LENGTH];
   unsigned libretro;
} rib_device_declaration;

typedef struct rib_controls_catalog
{
   rib_control_declaration entries[RIB_CONTROL_MAX];
   int count;
   rib_device_declaration devices[RIB_DEVICE_MAX];
   int device_count;
} rib_controls_catalog;

typedef struct rib_design_declarations rib_design_declarations;
/* One read of design.cfg. The returned data is immutable and borrowed until
 * free. For a missing file, the result is an empty declaration and a
 * diagnostic. In the menu we apply the stored toggle state separately. */
rib_design_declarations *rib_load_design(const char *asset_directory);
const rib_design_data *rib_design_get(const rib_design_declarations *design);
void rib_design_free(rib_design_declarations *design);

/* Read shaders.cfg when the menu loads. With a missing file or a missing
 * shader_ids field, the catalog is empty and we log no diagnostic. */
void rib_load_shaders(const char *asset_directory, rib_shader_catalog *catalog);

#ifdef __cplusplus
}
#endif
#endif
