#ifndef RIB_WIN32_QUIET_WINDOW_H
#define RIB_WIN32_QUIET_WINDOW_H

#include <windows.h>

#include <retro_inline.h>

#include "../../rominabox_session.h"

/* A test launch still requires a drawable. In an automated run we keep the
 * window out of the way of the person at the machine. It is fully
 * transparent and never activated, clicks pass through it, and it has no
 * taskbar button. With the explicit window switch, someone can check a game
 * by hand without changing the settings of the exported game. */
static INLINE void rominabox_prepare_test_window(HWND window)
{
   if (!window || !rib_session_window_hidden())
      return;
   SetWindowLongPtr(window, GWL_EXSTYLE, GetWindowLongPtr(window, GWL_EXSTYLE)
         | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW);
   SetLayeredWindowAttributes(window, 0, 0, LWA_ALPHA);
}

#endif
