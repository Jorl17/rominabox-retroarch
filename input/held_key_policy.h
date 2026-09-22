/* Whether to toggle the menu, used by the runloop and the held-key test.
 *
 * In one sample:
 *   - we count flushing down by one, even while another button is held.
 *     While it is still positive after this sample, we do not toggle.
 *   - we toggle for a press with both down and up since the last sample.
 *   - we still toggle while a direction is held.
 *
 * Returns 1 on the sample on which we toggle the menu.
 */
#ifndef HELD_KEY_POLICY_H
#define HELD_KEY_POLICY_H

void held_key_reset(void);
void held_key_note(unsigned code, int down);

int held_key_menu_toggle_fires(
      unsigned escape_code,
      int escape_level,
      int other_held,
      unsigned *flushing);

#endif
