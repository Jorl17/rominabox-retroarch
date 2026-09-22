#ifndef RIB_VOLUME_RANGE_H
#define RIB_VOLUME_RANGE_H

/* RetroArch's audio_volume, in decibels. We use this range for the hotkeys,
 * the settings list and the in-game slider alike. */
#define AUDIO_VOLUME_MIN_DB     (-80.0f)
#define AUDIO_VOLUME_MAX_DB     (12.0f)
#define AUDIO_VOLUME_STEP_DB    (1.0f)
#define AUDIO_VOLUME_DEFAULT_DB (0.0f)

/* The parts of the volume control. Each design has its own styles for
 * `slider` and `toggle`, and we use these names to mark the parts. */
#define RIB_VOLUME_SLIDER_ID "volume-level"
#define RIB_VOLUME_TOGGLE_ID "volume-mute"

/* The per-game file we append to the configuration at launch, and the
 * RetroArch keys in it. config_save_on_exit is off, so without this file we
 * would lose the player's choice when the game exits. */
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

/* Round to the nearest whole decibel, as in the RetroArch volume setting. */
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
