/* The player's settings: the RetroArch keys the menu can drive while the
 * game runs, and the kinds of control that show one. Declared once, here.
 *
 * The exporter (desktop/src-tauri/src/player_settings.rs) reads this file:
 * each setting it declares names its key and its kind by the first field,
 * and it writes the quoted word to design.cfg, so a key or kind this file
 * does not declare stops the export. The menu reads design.cfg's words back
 * through this list, and the host reads and applies each key through
 * rib_host_read_<name> and rib_host_apply_<name>, so a key the host cannot
 * apply stops the build.
 *
 * RIB_SETTING_KEY(name, "retroarch_config_key")
 * RIB_SETTING_KIND(name, "word") */
/* A reader defines the declarations it reads; the others read as nothing. */
#ifndef RIB_SETTING_KEY
#define RIB_SETTING_KEY(name, key)
#endif
#ifndef RIB_SETTING_KIND
#define RIB_SETTING_KIND(name, word)
#endif

/* The game's volume, in decibels. */
RIB_SETTING_KEY(AudioVolume, "audio_volume")
/* Whether the game pauses while its window is in the background. */
RIB_SETTING_KEY(PauseNonactive, "pause_nonactive")

/* A slider of evenly spaced positions between two values. */
RIB_SETTING_KIND(Level, "level")
/* On or off. */
RIB_SETTING_KIND(Switch, "switch")

#undef RIB_SETTING_KEY
#undef RIB_SETTING_KIND
