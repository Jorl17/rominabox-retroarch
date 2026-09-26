#ifndef RIB_VOLUME_RANGE_H
#define RIB_VOLUME_RANGE_H

/* RetroArch's audio_volume, in decibels. We keep the hotkeys, the settings
 * list in RetroArch and the in-game control between these ends.
 *
 * The top is unity gain, which is normal volume and the default, so the
 * player can lower the volume but not boost it. The bottom is silence, and
 * there is no separate mute. */
#define AUDIO_VOLUME_MIN_DB     (-80.0f)
#define AUDIO_VOLUME_MAX_DB     (0.0f)
#define AUDIO_VOLUME_DEFAULT_DB AUDIO_VOLUME_MAX_DB

/* The volumes the player can choose in the game menu, one per position,
 * silence first and normal last. Each step up sounds like an equal change.
 * Above silence, position p of 9 is 40*log10(p/9) dB, an amplitude of (p/9)
 * squared. We write each value to one decimal, as in the player's file. At
 * export we read this list and declare it to the menu. */
#define RIB_VOLUME_LEVELS_DB \
   AUDIO_VOLUME_MIN_DB, -38.2f, -26.1f, -19.1f, -14.1f, \
   -10.2f, -7.0f, -4.4f, -2.0f, AUDIO_VOLUME_MAX_DB

/* The sound we play for a volume change in a game exported without a menu
 * sound pack. It is next to the other menu assets. With a pack we play the
 * pack's movement sound instead, and we do not export this file. */
#define RIB_VOLUME_TICK_FILE "volume-tick.wav"

#endif
