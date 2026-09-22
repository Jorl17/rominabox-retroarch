#include "input/held_key_policy.h"

#include <string.h>

/* RETROK_LAST is 342. We make the array larger so that we ignore a stray
 * code instead of writing past the end. */
#define HELD_KEY_CODE_LIMIT 512

static unsigned char went_down[HELD_KEY_CODE_LIMIT];
static unsigned char went_up[HELD_KEY_CODE_LIMIT];
static int pending;

void held_key_reset(void)
{
   memset(went_down, 0, sizeof(went_down));
   memset(went_up, 0, sizeof(went_up));
   pending = 0;
}

void held_key_note(unsigned code, int down)
{
   if (code == 0 || code >= HELD_KEY_CODE_LIMIT)
      return;
   if (down)
      went_down[code] = 1;
   else
      went_up[code] = 1;
}

static void take(unsigned code, int *down, int *up)
{
   *down = 0;
   *up = 0;
   if (code == 0 || code >= HELD_KEY_CODE_LIMIT)
      return;
   *down = went_down[code];
   *up = went_up[code];
   went_down[code] = 0;
   went_up[code] = 0;
}

int held_key_menu_toggle_fires(
      unsigned escape_code,
      int escape_level,
      int other_held,
      unsigned *flushing)
{
   int edge_down = 0;
   int edge_up = 0;

   int fires = 0;

   take(escape_code, &edge_down, &edge_up);
   /* We have read the edges of this sample. Drop everything else, so that a
    * key pressed and released is not still pending at the next sample. */
   memset(went_down, 0, sizeof(went_down));
   memset(went_up, 0, sizeof(went_up));

   /* Ignore the toggle for two frames after the menu opens or closes. We
    * keep counting while a direction is held, so we still record Escape
    * while the player holds a direction. */
   if (*flushing > 0)
      (*flushing)--;
   if (*flushing > 0)
      return 0;

   /* A direction is not the key that opened the menu, so we still record
    * Escape while a direction is down. other_held is part of the sample,
    * but we do not use it for the result. */
   (void)other_held;

   /* A down and an up between two samples make a whole press. The key is
    * already up when we read the level, so we never detect such a press
    * from the level alone. */
   if (edge_down && edge_up)
   {
      pending = 0;
      fires = 1;
   }
   else if ((escape_level || edge_down) && !pending)
      pending = 1;
   else if (pending && (edge_up || !escape_level))
   {
      pending = 0;
      fires = 1;
   }
   return fires;
}
