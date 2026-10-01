/* The files the exporter writes for the player, and the keys of the two that
 * declare the design to it. Declared once, here. The player reads them by
 * these names (declarations.h); the exporter reads this file as the player's
 * compiler does (desktop/src-tauri/src/menu/declarations.rs), so a name
 * either side spells differently stops its build instead of exporting a
 * game that cannot find it.
 *
 * RIB_FILE(name, "file")              in the menu's directory
 * RIB_FILES(name, "prefix", "suffix") one per id there: prefix, id, suffix
 * RIB_DATA_FILE(name, "file")         in the game's data, written by the menu
 * RIB_KEY(name, "key")
 * RIB_KEYS(name, "prefix")            one per id: the prefix, then the id
 * RIB_CONTROLS_KEY(name, "key")       likewise, in the controls files, which
 * RIB_CONTROLS_KEYS(name, "prefix")   the launcher also reads */
/* A reader defines the declarations it reads; the others read as nothing. */
#ifndef RIB_FILE
#define RIB_FILE(name, file)
#endif
#ifndef RIB_FILES
#define RIB_FILES(name, prefix, suffix)
#endif
#ifndef RIB_DATA_FILE
#define RIB_DATA_FILE(name, file)
#endif
#ifndef RIB_KEY
#define RIB_KEY(name, key)
#endif
#ifndef RIB_KEYS
#define RIB_KEYS(name, prefix)
#endif
#ifndef RIB_CONTROLS_KEY
#define RIB_CONTROLS_KEY(name, key)
#endif
#ifndef RIB_CONTROLS_KEYS
#define RIB_CONTROLS_KEYS(name, prefix)
#endif

/* The menu's document. */
RIB_FILE(Menu, "menu.rml")
/* Each pad the picker offers, by its id: its scene, which the menu swaps in. */
RIB_FILES(Scene, "scene-", ".rml")
/* The pads' controls and the author's bindings. */
RIB_FILE(ControlsDefaults, "controls-defaults.cfg")
/* The player's own bindings, and the filter the player last chose. */
RIB_DATA_FILE(Controls, "controls.cfg")
RIB_DATA_FILE(ShaderChoice, "shader-choice")

/* The controls files, beside RetroArch's own input_player1_ bindings: the pad
 * in use, the pads the picker offers, each pad's name, emulated device and
 * controls, and each control's label, the group it is drawn in, and the pad
 * position it is read from when that is not its own. */
RIB_CONTROLS_KEY(ControlsProfile, "controls_profile")
RIB_CONTROLS_KEY(ControlsVariants, "controls_variants")
RIB_CONTROLS_KEYS(ControlsVariantName, "controls_variant_name_")
RIB_CONTROLS_KEYS(ControlsVariantDevice, "controls_variant_device_")
RIB_CONTROLS_KEYS(ControlsVariantControls, "controls_variant_controls_")
RIB_CONTROLS_KEYS(ControlLabel, "rib_label_")
RIB_CONTROLS_KEYS(ControlGroup, "rib_group_")
RIB_CONTROLS_KEYS(ControlPosition, "rib_position_")

/* design.cfg: the design's screens, settings, overlays, bind list, fonts and
 * words. A list is ids separated by spaces. */
RIB_FILE(Design, "design.cfg")
RIB_KEY(Screens, "screens")
RIB_KEYS(ScreenPanel, "screen_panel_")
RIB_KEYS(ScreenHeading, "screen_heading_")
RIB_KEYS(ScreenFooter, "screen_footer_")
RIB_KEYS(ScreenButton, "screen_button_")
RIB_KEYS(ScreenImages, "screen_images_")
RIB_KEYS(ScreenRole, "screen_role_")
RIB_KEY(Settings, "settings")
RIB_KEYS(SettingControl, "setting_control_")
RIB_KEYS(SettingKey, "setting_key_")
RIB_KEYS(SettingFile, "setting_file_")
RIB_KEYS(SettingKind, "setting_kind_")
RIB_KEYS(SettingValues, "setting_values_")
RIB_KEYS(SettingInverted, "setting_inverted_")
RIB_KEYS(SettingDefault, "setting_default_")
RIB_KEY(Overlays, "overlays")
RIB_KEYS(OverlayFollows, "overlay_follows_")
RIB_KEYS(OverlayAfter, "overlay_after_")
RIB_KEYS(OverlayHold, "overlay_hold_")
RIB_KEYS(OverlayLeave, "overlay_leave_")
RIB_KEYS(OverlayNeeds, "overlay_needs_")
RIB_KEYS(OverlayHoldsGame, "overlay_holds_game_")
RIB_KEY(BindsAfter, "binds_after")
RIB_KEY(BindsHoverAfter, "binds_hover_after")
RIB_KEY(BindsWidth, "binds_width")
RIB_KEY(BindsList, "binds_list")
RIB_KEY(Fonts, "fonts")
/* A word of words.inc in the design's wording, by the word's id. */
RIB_KEYS(Word, "word_")

/* HOTKEYS: the author's bindings for the hotkeys (hotkeys.inc), with the
 * words the menu shows for each pad input, and the player's own bindings,
 * which win for every hotkey once the player has changed any. A list is
 * bindings separated by spaces. */
RIB_FILE(HotkeysDefaults, "hotkeys-defaults.cfg")
RIB_DATA_FILE(Hotkeys, "hotkeys.cfg")
RIB_KEYS(HotkeyList, "hotkey_")
RIB_KEYS(PadWord, "pad_word_")

/* shaders.cfg: the filters a game bundles, in order, and each one's preset. */
RIB_FILE(Shaders, "shaders.cfg")
RIB_KEY(ShaderIds, "shader_ids")
RIB_KEYS(ShaderPreset, "shader_preset_")

#undef RIB_FILE
#undef RIB_FILES
#undef RIB_DATA_FILE
#undef RIB_KEY
#undef RIB_KEYS
#undef RIB_CONTROLS_KEY
#undef RIB_CONTROLS_KEYS
