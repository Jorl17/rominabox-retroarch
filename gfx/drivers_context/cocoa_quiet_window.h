#ifndef RIB_COCOA_QUIET_WINDOW_H
#define RIB_COCOA_QUIET_WINDOW_H

#include <stdlib.h>
#include <string.h>
#import <AppKit/AppKit.h>

/* A test launch still requires a drawable. With the explicit window switch,
 * someone can check a game by hand without changing its exported settings. */
static inline bool rominabox_test_window_shown(void)
{
   const char *shown = getenv("ROMINABOX_SHOW_WINDOW");
   return shown && strcmp(shown, "1") == 0;
}

static inline bool rominabox_test_window_hidden(void)
{
   return getenv("ROMINABOX_QUIET") && !rominabox_test_window_shown();
}

static inline void rominabox_prepare_test_window(NSWindow *window)
{
   if (!window || !rominabox_test_window_hidden())
      return;
   /* An off-screen position alone does not hide the window, because a
    * titled window may move back when we order it in or resize it. Keep
    * its drawable, but never show the window in an automated run. */
   [window setAlphaValue:0.0];
   [window setIgnoresMouseEvents:YES];
}

#endif
