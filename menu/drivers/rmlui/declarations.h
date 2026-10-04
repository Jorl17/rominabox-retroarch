#ifndef RIB_MENU_DECLARATIONS_H
#define RIB_MENU_DECLARATIONS_H

#include <stdbool.h>
#include <stddef.h>
#include <retro_miscellaneous.h>

#ifdef __cplusplus
#include "host.h"
#include "screen_role.hpp"
#include "words.hpp"
#include <string>
#include <vector>

namespace rib {
/* The files we write for the menu at export, and their keys, as listed in
 * declarations.inc. */
namespace files {
#define RIB_FILE(name, file) inline constexpr char name[] = file;
#define RIB_DATA_FILE(name, file) inline constexpr char name[] = file;
#define RIB_FILES(name, prefix, suffix) \
   inline std::string name(const std::string& id) { return prefix + id + suffix; }
#include "declarations.inc"
}
namespace keys {
#define RIB_KEY(name, key) inline constexpr char name[] = key;
#define RIB_KEYS(name, prefix) \
   inline std::string name(const std::string& id) { return prefix + id; }
#define RIB_CONTROLS_KEY(name, key) inline constexpr char name[] = key;
#define RIB_CONTROLS_KEYS(name, prefix) \
   inline std::string name(const std::string& id) { return prefix + id; }
#include "declarations.inc"
}

/* design.cfg, which we read once when we load the design. We keep every value
 * whole and never cut what we wrote at export to make it fit. */
struct ScreenDeclaration
{
   std::string id, panel, heading, footer;
   /* The elements that open this screen when the player presses them. */
   std::vector<std::string> buttons;
   /* The screen we show instead when the core has loaded several images. */
   std::string images;
   ScreenRole role = ScreenRole::None;
};

struct OverlayDeclaration
{
   std::string id, follows, needs;
   int after_ms = 0, hold_ms = 0, leave_ms = 0;
   /* The game starts only once this overlay has gone (the splash). */
   bool holds_game = false;
};

/* How a player setting is shown, as settings.inc declares the kinds. */
enum class SettingKind
{
#define RIB_SETTING_KIND(name, word) name,
#include "settings.inc"
};

/* A key's RetroArch config name, as settings.inc declares it. */
const char *setting_key_name(rib_setting_key key);

/* A setting the player changes in the Options of the game, for one RetroArch
 * key. We declare every one at export. In the menu we show it in `control`,
 * apply it at once and store it in `file`, in the game's data, and at the
 * next launch we apply that file in the launcher. */
struct SettingDeclaration
{
   std::string id, control, file;
   rib_setting_key key = RIB_SETTING_KEY_COUNT;
   SettingKind kind = SettingKind::Level;
   /* A level: the value of the key at each position, from the low end of the
    * slider to the high end. A choice: its positions, in the order in which
    * a press moves through them. */
   std::vector<float> values;
   /* A switch: whether on is the key's false. The words for it are in the
    * menu (Word::SwitchOn, Word::SwitchOff). */
   bool inverted = false;
   /* The value of the key when the game starts, before the player changes
    * it: the value of a level, or 1 or 0 for a switch. */
   float default_value = 0.0f;
};

/* When the list of a control's bindings appears, and how wide it is. */
struct BindsDeclaration
{
   std::string list;
   int after_ms = 0;         /* after a key reaches a control */
   int hover_after_ms = 300; /* after the pointer comes to rest on one */
   int width = 0;
};

struct DesignDeclarations
{
   std::vector<ScreenDeclaration> screens;
   std::vector<OverlayDeclaration> overlays;
   std::vector<SettingDeclaration> settings;
   /* The font files staged next to the document, for drawing the menu. */
   std::vector<std::string> fonts;
   BindsDeclaration binds;
   /* The words whose wording is in the design. */
   std::vector<std::pair<Word, std::string>> words;
};

/* One read of `asset_directory`/design.cfg. For a missing file the declaration
 * is empty, and we say so in the log. */
DesignDeclarations load_design(const char *asset_directory);
}
#endif

#ifdef __cplusplus
/* The fixed bounds of the exported format: the maximum count of each item.
 * We read every id, name and list in full, whatever its length. */
enum { RIB_SHADER_MAX = 32, RIB_CONTROL_MAX = 48, RIB_DEVICE_MAX = 8 };

/* One exported shader id and its preset. We keep the order of the shader
 * file, and we include empty presets. */
struct rib_shader_declaration
{
   std::string id;
   std::string preset;
};

struct rib_shader_catalog
{
   rib_shader_declaration entries[RIB_SHADER_MAX];
   int count = 0;
};

/* Controls and controller variants in the exported declaration order. We
 * borrow this catalog in the view and never call into the menu for items. */
struct rib_control_declaration
{
   std::string id;
   std::string group;
   /* The pad position we read it from, and so the bind under which we store
    * its keys and buttons: its id unless the author moved it. */
   std::string slot;
   unsigned bind_index = 0;
   bool enabled = false;
   std::string label;
};

struct rib_device_declaration
{
   std::string id;
   std::string name;
   unsigned libretro = 0;
};

struct rib_controls_catalog
{
   rib_control_declaration entries[RIB_CONTROL_MAX];
   int count = 0;
   rib_device_declaration devices[RIB_DEVICE_MAX];
   int device_count = 0;
};

struct config_file;
/* Open one controls file and read its profile and, for defaults, its declared
 * controls/devices. Keep the file open during host bind loading, and free it
 * with config_file_free. We look up only exported bind ids in the fixed host
 * table for bind_index. Existing catalog slots keep their ordinal state, apart
 * from the fields that discovery writes again. `profile_id` is the pad in the
 * file, unchanged when there is none. */
struct config_file *rib_open_controls(const char *path, bool defaults,
      std::string& profile_id, rib_controls_catalog *catalog,
      bool *profile_present, bool (*bind_index)(const char *, unsigned *));
/* Read the open defaults again, for another pad in them. Returns false, with
 * the catalog unchanged, when there is no `profile` in them. */
bool rib_controls_discover(struct config_file *defaults, const char *profile,
      rib_controls_catalog *catalog, bool (*bind_index)(const char *, unsigned *));
/* Read the active-bind flags in the defaults, then each active label at its
 * point between host clear and host load. */
void rib_controls_read_enabled(struct config_file *config,
      rib_controls_catalog *catalog);
void rib_controls_read_label(struct config_file *config,
      rib_control_declaration *control);

/* Read shaders.cfg when the menu loads. With a missing file or a missing
 * shader_ids field, the catalog is empty and we log no diagnostic. */
void rib_load_shaders(const char *asset_directory, rib_shader_catalog *catalog);
#endif
#endif
