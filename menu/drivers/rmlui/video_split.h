/* ROM-in-a-Box: how we split the brightness that the player chose between the
 * brightness parameter of the chosen shader and our pass (video.c). We keep it
 * apart from RetroArch's shaders, so that a test can call it. */
#ifndef RIB_VIDEO_SPLIT_H
#define RIB_VIDEO_SPLIT_H

/* The most positions in the table of a brightness parameter of a shader. */
#define RIB_VIDEO_CONTROL_POSITIONS 16
/* The longest path of a preset that we keep. */
#define RIB_VIDEO_PRESET_LENGTH 4096

/* The brightness parameter of a bundled shader, and the light at each value,
 * from 1.0 up, as we give them in shaders.cfg. */
struct rib_video_control
{
   char preset[RIB_VIDEO_PRESET_LENGTH];
   char parameter[64];
   float light[RIB_VIDEO_CONTROL_POSITIONS];
   float value[RIB_VIDEO_CONTROL_POSITIONS];
   unsigned count;
};

/* How we reach the brightness the player chose with the chosen shader, as
 * the value of its brightness parameter, when there is one, and the
 * multiplier left for our pass. */
struct rib_video_split
{
   const char *parameter;
   float value;
   float pass;
};

/* Read `text`, the parameter and then light:value at each value, into
 * `control` for the preset `preset`. Returns 0 for a table with fewer than
 * two values, which we cannot use. */
int rib_video_control_read(struct rib_video_control *control,
      const char *preset, const char *text);

/* `brightness` split for the chosen preset `chosen`, by the first of the
 * `count` controls with that preset, up to the top of its table through its
 * parameter, and the rest through our pass. At or below 1.0 we give the
 * parameter its value in the preset, the first in its table, and dim with our
 * pass, so that a running shader we raised before does not stay raised. */
struct rib_video_split rib_video_split_brightness(
      const struct rib_video_control *controls, unsigned count,
      const char *chosen, float brightness);

#endif
