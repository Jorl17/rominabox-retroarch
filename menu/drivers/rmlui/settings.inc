/* The player's settings: the RetroArch keys the menu can drive while the
 * game runs, and the kinds of control that show one. Declared once, here.
 *
 * The exporter (desktop/src-tauri/src/player_settings.rs) reads this file:
 * each setting it declares names its key and its kind by the first field,
 * and it writes the quoted word to design.cfg, so a key or kind this file
 * does not declare stops the export. The menu reads design.cfg's words back
 * through this list, and the host reads and applies each key, and says
 * whether the running game has any use for it, through rib_host_read_<name>,
 * rib_host_apply_<name> and rib_host_used_<name>, so a key the host cannot
 * apply stops the build.
 *
 * RIB_SETTING_KEY(name, "retroarch_config_key")
 * RIB_SETTING_KIND(name, "word")
 * RIB_SETTING_POSITIONS(name, "value value ...")
 *    A level's values, one per position from its low end to its high end,
 *    as RetroArch's config writes them. */
/* A reader defines the declarations it reads; the others read as nothing. */
#ifndef RIB_SETTING_KEY
#define RIB_SETTING_KEY(name, key)
#endif
#ifndef RIB_SETTING_KIND
#define RIB_SETTING_KIND(name, word)
#endif
#ifndef RIB_SETTING_POSITIONS
#define RIB_SETTING_POSITIONS(name, values)
#endif

/* The game's volume, in decibels. */
RIB_SETTING_KEY(AudioVolume, "audio_volume")
/* Whether the game pauses while its window is in the background. */
RIB_SETTING_KEY(PauseNonactive, "pause_nonactive")
/* Whether a pad rumbles when the game asks it to. */
RIB_SETTING_KEY(InputRumbleEnable, "input_rumble_enable")
/* How fast fast forward runs the game: twice, four times, or as fast as the
 * computer can (RetroArch's 0). */
RIB_SETTING_KEY(FastforwardRatio, "fastforward_ratio")
RIB_SETTING_POSITIONS(FastforwardRatio, "2.0 4.0 0.0")

/* A slider of positions, each with the value the exporter declares for it. */
RIB_SETTING_KIND(Level, "level")
/* On or off. */
RIB_SETTING_KIND(Switch, "switch")

#undef RIB_SETTING_KEY
#undef RIB_SETTING_KIND
#undef RIB_SETTING_POSITIONS
