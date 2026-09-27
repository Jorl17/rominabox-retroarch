/* The environment variables that we pass from the ROM-in-a-Box launcher to
 * the player. We set them in the launcher (desktop/src-tauri/launcher/main.c)
 * and read them in the player and in its accounts store. Both sides include
 * this file, so a name spelled differently on one side stops the build, and
 * no game is released with a missing value. The file has only names and no
 * includes, so it also compiles in the launcher, built without RetroArch. */
#ifndef ROMINABOX_LAUNCH_H
#define ROMINABOX_LAUNCH_H

/* The game's own data folder, absolute. */
#define RIB_ENV_DATA_DIR "ROMINABOX_DATA_DIR"
/* The game's identity, the name of its data folder. */
#define RIB_ENV_GAME_IDENTITY "ROMINABOX_GAME_IDENTITY"
/* The game's title, for its window and menu bar. */
#define RIB_ENV_TITLE "ROMINABOX_TITLE"
/* The composed menu's folder inside the app. */
#define RIB_ENV_RML_ASSETS "ROMINABOX_RML_ASSETS"
/* "1" when the builder unlocked the full emulator behind Advanced. */
#define RIB_ENV_ADVANCED_ACCESS "ROMINABOX_ADVANCED_ACCESS"
/* Set when the game opens at its menu. */
#define RIB_ENV_START_AT_MENU "ROMINABOX_START_AT_MENU"
/* "1" when the game has achievements. */
#define RIB_ENV_ACHIEVEMENTS "ROMINABOX_ACHIEVEMENTS"
/* The folder for QUICK SIGN IN, set only when the export has one. */
#define RIB_ENV_ACCOUNTS_DIR "ROMINABOX_ACCOUNTS_DIR"
/* An automated run: no sound, and the window stays out of the way. We set it
 * in the launcher for any launch not started by Launch Services. */
#define RIB_ENV_QUIET "ROMINABOX_QUIET"
/* A hands-on test: "1" shows the window of an automated run. */
#define RIB_ENV_SHOW_WINDOW "ROMINABOX_SHOW_WINDOW"
/* A screenshot run: where the menu's picture goes. */
#define RIB_ENV_MENU_SHOT "ROMINABOX_MENU_SHOT"
/* The program a person opens this game with, absolute, so that a Windows
 * taskbar button pinned from the running game opens it again. We set it only
 * in the Windows launcher, because the game window belongs to the player. */
#define RIB_ENV_RELAUNCH "ROMINABOX_RELAUNCH"

#endif
