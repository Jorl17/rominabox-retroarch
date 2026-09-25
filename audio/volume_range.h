#ifndef RIB_VOLUME_RANGE_H
#define RIB_VOLUME_RANGE_H

/* RetroArch's audio_volume, in decibels. We use these ends for the hotkeys
 * and for the in-game control, so both stop at the same loudest value.
 *
 * The top is unity gain, which is normal volume and the default, so the
 * player can lower the volume but not boost it. The bottom is silence, and
 * there is no separate mute.
 *
 * We offer ten positions across that range in the menu, not one step per
 * decibel, and the top one is normal volume. We write the step out as a
 * literal because, when we check this header, we parse only digits and
 * cannot evaluate a division. With this float value, nine steps reach
 * -80 exactly. */
#define RIB_VOLUME_POSITIONS    10
#define AUDIO_VOLUME_MIN_DB     (-80.0f)
#define AUDIO_VOLUME_MAX_DB     (0.0f)
#define AUDIO_VOLUME_DEFAULT_DB (0.0f)
#define AUDIO_VOLUME_STEP_DB    (8.888888889f)

/* The sound we play for a volume change in a game exported without a menu
 * sound pack. It is next to the other menu assets. With a pack we play the
 * pack's movement sound instead, and we do not export this file. */
#define RIB_VOLUME_TICK_FILE "volume-tick.wav"

#endif
