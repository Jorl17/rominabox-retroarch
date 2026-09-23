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
   CGFloat right = 0;
   for (NSScreen *screen in [NSScreen screens])
      right = MAX(right, NSMaxX([screen frame]));
   [window setFrameOrigin:NSMakePoint(right + 32, 0)];
}

#endif
