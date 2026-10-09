/* When the player presses Alt+Enter, or Option+Return on a Mac, we toggle
 * fullscreen (rib_host_toggle_fullscreen).
 *
 * We write input_toggle_fullscreen as nul in an exported game, because f
 * is a gameplay key and a config bind cannot be the chord. The key comes
 * here as Return with RETROKMOD_ALT.
 *
 * Enter alone is still Start. Holding the chord toggles only once.
 */
#ifndef ALT_ENTER_FULLSCREEN_H
#define ALT_ENTER_FULLSCREEN_H

/* Values from libretro.h: RETROK_RETURN, RETROK_KP_ENTER, RETROKMOD_ALT.
 * The build fails in input_driver.c when they differ. */
#define ALT_ENTER_RETURN 13u
#define ALT_ENTER_KP_RETURN 271u
#define ALT_ENTER_ALT 0x04u

/* The Return of the chord, down or up. In the menu text entry we use it for
 * the fullscreen toggle, and do not press the focused button with it. */
static inline int alt_enter_is_chord(unsigned code, unsigned modifiers)
{
   return (code == ALT_ENTER_RETURN || code == ALT_ENTER_KP_RETURN)
         && (modifiers & ALT_ENTER_ALT);
}

void alt_enter_reset(void);
/* A press of `code` that the system says is no repeat. On macOS the key-up
 * of Return can be lost while the window moves into or out of full screen,
 * and without it the next press would pass for a repeat. */
void alt_enter_fresh_press(unsigned code);
void alt_enter_note(unsigned code, int down, unsigned modifiers);
int alt_enter_fullscreen_due(void);
int alt_enter_masks_return(void);

#endif
