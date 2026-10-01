/* The menu script's steps that are neither `name:value` nor an element to
 * click (script.cpp). Declared once, here: the workflow tests read this file
 * to tell them from the ids a script clicks
 * (desktop/src-tauri/src/menu/script.rs).
 *
 * RIB_SCRIPT_COMMAND(Name, "step") */

/* Escape's command: the menu opens, or closes. */
RIB_SCRIPT_COMMAND(Toggle, "toggle")
/* Alt+Enter's command: the video driver restarts for the screen, or for the
 * window. */
RIB_SCRIPT_COMMAND(Fullscreen, "fullscreen")
/* The next step waits until the design's overlays have come and gone. */
RIB_SCRIPT_COMMAND(WaitOverlays, "wait-overlays")
