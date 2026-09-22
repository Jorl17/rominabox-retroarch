#ifndef RIB_VOLUME_RANGE_H
#define RIB_VOLUME_RANGE_H

/* RetroArch's audio_volume, in decibels. We use these ends for the hotkeys
 * and for the in-game control, so both stop at the same loudest value.
 *
 * The top is unity gain, which is normal volume and the default, so the
 * player can lower the volume but not boost it. The bottom is silence, and
 * there is no separate mute.
 *
 * We offer a few positions across that range in the menu, not one step per
 * decibel. The step is the gap between those positions, so a hotkey and an
 * arrow key move to the same positions. */
#define RIB_VOLUME_POSITIONS    5
#define AUDIO_VOLUME_MIN_DB     (-80.0f)
#define AUDIO_VOLUME_MAX_DB     (0.0f)
#define AUDIO_VOLUME_DEFAULT_DB (0.0f)
#define AUDIO_VOLUME_STEP_DB    (20.0f)

/* The parts of the volume control. Each design has its own style for
 * `slider`, and we use these names to mark which part is which. */
#define RIB_VOLUME_SLIDER_ID "volume-level"
#define RIB_VOLUME_DOWN_ID   "volume-down"
#define RIB_VOLUME_UP_ID     "volume-up"
#define RIB_VOLUME_LOW_ID    "volume-low"
#define RIB_VOLUME_HIGH_ID   "volume-high"

/* The per-game file we append to the configuration at launch, and the
 * RetroArch key in it. config_save_on_exit is off, so without this file we
 * would lose the player's choice when the game exits. We read a file that
 * sets audio_mute_enable as the lowest position. */
#define RIB_VOLUME_FILE     "volume.cfg"
#define RIB_VOLUME_KEY      "audio_volume"
#define RIB_VOLUME_MUTE_KEY "audio_mute_enable"

static inline float rib_volume_clamp_db(float db)
{
   if (db < AUDIO_VOLUME_MIN_DB)
      return AUDIO_VOLUME_MIN_DB;
   if (db > AUDIO_VOLUME_MAX_DB)
      return AUDIO_VOLUME_MAX_DB;
   return db;
}

/* Round to the nearest position. The step is the gap between the menu
 * positions, not one decibel, so a drag always ends on a position. */
static inline float rib_volume_quantize_db(float db)
{
   float steps;

   db = rib_volume_clamp_db(db);
   steps = (db - AUDIO_VOLUME_MIN_DB) / AUDIO_VOLUME_STEP_DB;
   steps = (float)(int)(steps + 0.5f);
   return rib_volume_clamp_db(
         AUDIO_VOLUME_MIN_DB + steps * AUDIO_VOLUME_STEP_DB);
}

static inline float rib_volume_fraction_from_db(float db)
{
   db = rib_volume_clamp_db(db);
   return (db - AUDIO_VOLUME_MIN_DB)
         / (AUDIO_VOLUME_MAX_DB - AUDIO_VOLUME_MIN_DB);
}

static inline float rib_volume_db_from_fraction(float fraction)
{
   if (fraction < 0.0f)
      fraction = 0.0f;
   if (fraction > 1.0f)
      fraction = 1.0f;
   return rib_volume_quantize_db(AUDIO_VOLUME_MIN_DB
         + fraction * (AUDIO_VOLUME_MAX_DB - AUDIO_VOLUME_MIN_DB));
}

#endif
