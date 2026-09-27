/* What the ROM-in-a-Box launcher set for this run, read once for every
 * frontend. We apply these facts in Cocoa and Win32, and read nothing for
 * them from the launcher environment there. */
#ifndef RIB_SESSION_H
#define RIB_SESSION_H

#include <boolean.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* The game's title, UTF-8, when the player runs as an exported game, or NULL
 * when it runs as RetroArch. We use it as the window title and keep it, so the
 * title updates in RetroArch do not replace it. */
const char *rib_session_title(void);

/* The game's identity, the name of its data folder, when the player runs as
 * an exported game, or NULL when it runs as RetroArch. */
const char *rib_session_identity(void);

/* The builder unlocked the full emulator behind Advanced. */
bool rib_session_advanced_access(void);

/* An exported game whose builder did not unlock the full emulator. We offer
 * neither the emulator menus nor opening or dropping files. */
bool rib_session_restricted(void);

/* An automated run. The window exists, for its drawable, but we never put it in
 * front of the person at the machine or move the focus to it. */
bool rib_session_window_hidden(void);

/* The tester asked to see the window of an automated run. */
bool rib_session_window_shown(void);

/* A run that is only here to photograph the menu. */
bool rib_session_menu_shot(void);

/* The program to open this game again, UTF-8, when the game window is a
 * window of another program than the one a person opened (Windows), or NULL
 * otherwise. Valid for the whole process. */
const char *rib_session_relaunch(void);

RETRO_END_DECLS

#endif
