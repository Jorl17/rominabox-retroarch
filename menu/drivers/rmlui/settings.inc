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
 *    as RetroArch's config writes them.
 * RIB_SETTING_PERCENT(name)
 *    A level whose value we show in the readout of its slider as a
 *    percentage, 1.2 as 120%.
 * RIB_SETTING_PARAMETER(name, "PARAMETER")
 *    A key we apply as a parameter of our pass after the game's shader
 *    (video.c), and the parameter's name in that pass. In the exporter we
 *    write the pass with these names. */
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
#ifndef RIB_SETTING_PERCENT
#define RIB_SETTING_PERCENT(name)
#endif
#ifndef RIB_SETTING_PARAMETER
#define RIB_SETTING_PARAMETER(name, parameter)
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
/* The light of the game's picture, as a multiplier, from half to double in
 * steps of about a quarter of a stop, and its contrast around mid-grey. */
RIB_SETTING_KEY(VideoBrightness, "rib_video_brightness")
RIB_SETTING_POSITIONS(VideoBrightness, "0.5 0.6 0.7 0.85 1.0 1.2 1.4 1.7 2.0")
RIB_SETTING_PERCENT(VideoBrightness)
RIB_SETTING_PARAMETER(VideoBrightness, "RIB_BRIGHTNESS")
RIB_SETTING_KEY(VideoContrast, "rib_video_contrast")
RIB_SETTING_POSITIONS(VideoContrast, "0.67 0.74 0.82 0.9 1.0 1.11 1.22 1.36 1.5")
RIB_SETTING_PERCENT(VideoContrast)
RIB_SETTING_PARAMETER(VideoContrast, "RIB_CONTRAST")

/* A slider of positions, each with the value the exporter declares for it. */
RIB_SETTING_KIND(Level, "level")
/* On or off. */
RIB_SETTING_KIND(Switch, "switch")
/* One of its positions, an Options entry a press moves to the next, the
 * last back to the first; each position's word is <id>-<n> (words.inc). */
RIB_SETTING_KIND(Choice, "choice")

#undef RIB_SETTING_KEY
#undef RIB_SETTING_KIND
#undef RIB_SETTING_POSITIONS
#undef RIB_SETTING_PERCENT
#undef RIB_SETTING_PARAMETER
