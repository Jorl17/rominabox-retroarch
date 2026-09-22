#ifndef RMLUI_BRIDGE_H
#define RMLUI_BRIDGE_H

#include <stdbool.h>
#include <string.h>

#include "../../audio/volume_range.h"

#ifdef __cplusplus
extern "C" {
#endif

enum rib_rmlui_action
{
   RIB_RMLUI_ACTION_NONE = 0,
   RIB_RMLUI_ACTION_RESUME,
   RIB_RMLUI_ACTION_SAVE,
   RIB_RMLUI_ACTION_LOAD,
   RIB_RMLUI_ACTION_CONTROLS,
   RIB_RMLUI_ACTION_QUIT,
   RIB_RMLUI_ACTION_SELECT_SLOT_1,
   RIB_RMLUI_ACTION_SELECT_SLOT_2,
   RIB_RMLUI_ACTION_SELECT_SLOT_3,
   RIB_RMLUI_ACTION_SELECT_SLOT_4,
   RIB_RMLUI_ACTION_SELECT_SLOT_5,
   RIB_RMLUI_ACTION_SELECT_SLOT_6,
   RIB_RMLUI_ACTION_CONTROLS_BACK,
   RIB_RMLUI_ACTION_CONTROLS_RESET,
   RIB_RMLUI_ACTION_CONTROLS_CANCEL,
   RIB_RMLUI_ACTION_CONTROL_FIRST,
   /* The size of a buffer: how many control actions fit in the mailbox, not
    * which controls exist. It must be enough for every control a console
    * declares, and a PlayStation DualShock declares twenty-four. In the menu
    * we read the controls declared for the console from rmlui.c, through
    * rib_rmlui_control_id. */
   RIB_RMLUI_ACTION_CONTROL_LAST = RIB_RMLUI_ACTION_CONTROL_FIRST + 47,

   /* Opening and closing the controller picker. We pass the chosen option
    * as a string next to the action, not as another range of enum values like
    * "select slot N", so we do not add a range for every new list in the
    * menu. */
   RIB_RMLUI_ACTION_DEVICE_PICKER_TOGGLE,
   RIB_RMLUI_ACTION_DEVICE_PICKER_CHOOSE,
   /* The player moved a slider or a toggle. We pass the part and the value
    * next to the action, as for a screen id, so the enum never gets a case
    * for each control. */
   RIB_RMLUI_ACTION_SLIDER,
   /* We paint a part toggle where it is. For a list switch we pass its name
    * through rib_rmlui_chosen_item() and store it here. One action for both,
    * because a second switch is a line in a design, not another member. */
   RIB_RMLUI_ACTION_TOGGLE,
   /* The player chose a row in a generated list, or moved its pager. We pass
    * the row and the direction next to the action. Lists add nothing to the
    * enum, because shaders and achievements use the same kind of row. */
   RIB_RMLUI_ACTION_LIST_CHOOSE,
   RIB_RMLUI_ACTION_LIST_PAGE,
   /* The player asked for a declared screen. We pass which one next to the
    * action, through rib_rmlui_requested_screen(), so the number of screens
    * in a design is never part of this enum. We do the same for the
    * controller picker, for the same reason. */
   RIB_RMLUI_ACTION_SHOW_SCREEN
};

/* How many control ids there are in the player, and each id in the order of
 * the console declaration. The code is in rmlui.c, where we read the list
 * from the exported controls configuration. On purpose, there are no control
 * names in the bridge. */
/* Attach listeners to control elements. Call it after we know the control
 * list. We load the document before that, so at load time there is nothing
 * to attach to. */
void rib_rmlui_wire_controls(void);
void rib_rmlui_wire_device_picker(void);
void rib_rmlui_set_device_picker(bool open, const char *chosen);

/* The controllers in the picker, which we read in the player from the exported
 * configuration, and the one in use now. There are no controller names in the
 * bridge, for the same reason there are no control names. */
int rib_rmlui_device_count(void);
const char *rib_rmlui_device_id(int index);
const char *rib_rmlui_device_name(int index);

/* The option the player chose with a click, valid until the next action. */
const char *rib_rmlui_chosen_device(void);

int rib_rmlui_control_capacity(void);
const char *rib_rmlui_control_id(int index);

/* Escape/menu-toggle: cancel capture, then leave Controls, then resume. */
static inline int rib_rmlui_map_menu_toggle(
      bool controls_visible, bool capture_active)
{
   if (capture_active)
      return RIB_RMLUI_ACTION_CONTROLS_CANCEL;
   if (controls_visible)
      return RIB_RMLUI_ACTION_CONTROLS_BACK;
   return RIB_RMLUI_ACTION_RESUME;
}

static inline bool rib_rmlui_toggle_stays_in_menu(
      bool controls_visible, bool capture_active)
{
   return capture_active || controls_visible;
}

static inline bool rib_rmlui_ok_includes_pointer_select(bool is_rmlui)
{
   return !is_rmlui;
}

static inline bool rib_rmlui_load_is_actionable(bool occupied)
{
   return occupied;
}

/* Exact requested path/slot and save-vs-load. Missing identity is not a wildcard. */
static inline bool rib_rmlui_state_task_matches(
      bool pending, bool pending_is_save, const char *pending_path,
      int pending_slot, const char *path, int slot, bool is_save)
{
   if (!pending || pending_is_save != is_save)
      return false;
   if (path && path[0] && pending_path && pending_path[0]
         && strcmp(pending_path, path) != 0)
      return false;
   if (slot >= 0 && pending_slot >= 0 && slot != pending_slot)
      return false;
   return true;
}

bool rib_rmlui_init(const char *asset_directory, int width, int height);
void rib_rmlui_shutdown(void);
void rib_rmlui_render(int width, int height);
void rib_rmlui_set_selected_slot(int slot);
void rib_rmlui_set_focused(int focused);
void rib_rmlui_set_slot_state(int slot, bool occupied,
      const char *thumbnail_path);
void rib_rmlui_set_game_aspect(float aspect);
void rib_rmlui_set_status(const char *status);
void rib_rmlui_show_controls(bool visible);
void rib_rmlui_set_control_state(const char *id, const char *label,
      const char *binding, bool focused, bool capturing);
void rib_rmlui_set_controls_action_focus(
      bool reset, bool back, bool cancel);
void rib_rmlui_set_controls_status(const char *status);
void rib_rmlui_set_footer_hint(const char *hint);

/* How we draw an overlay at this moment.
 *
 * An overlay is not a screen. Nobody asks for it by name, it has no input, it
 * hides nothing, and it is over the game and not inside the menu. In the
 * player we set only which of these states an element is in. How each state
 * appears, and how long leaving lasts, is in the stylesheet of the design. */
enum rib_overlay_state
{
   RIB_OVERLAY_HIDDEN = 0,
   RIB_OVERLAY_SHOWING,
   RIB_OVERLAY_LEAVING
};

/* The document drawn over a running game and not in front of it. What that
 * means for the frame, heading and panels of the menu is up to the design, so
 * there are no element ids here. */
void rib_rmlui_set_overlay_mode(bool only_overlays);
void rib_rmlui_set_overlay(const char *element, enum rib_overlay_state state);
void rib_rmlui_pointer_move(int x, int y);
void rib_rmlui_pointer_button(bool down);
void rib_rmlui_pointer_leave(void);
int rib_rmlui_take_action(void);

/* Click an element by id, as with a pointer. False when there is no such element
 * in the document. Treat that as a failure, because after clicking nothing,
 * a screenshot would show the wrong thing. */
bool rib_rmlui_click_element(const char *id);

/* Write the NEXT rendered menu frame to this path, then stop. We capture it
 * where the pixels are, after we draw the menu over the frame of the core and
 * before the buffer is presented. A read from the runloop returns an empty
 * buffer, so the result is a black picture written without an error. */
void rib_rmlui_capture_next(const char *path);

/* The screens declared in a design. We clear and declare them again when we
 * load the document, and there are no screens in the player itself. */
void rib_rmlui_clear_screens(void);
void rib_rmlui_declare_screen(const char *id, const char *panel,
      const char *heading, const char *footer, const char *button);

/* Show one declared screen and hide the rest. False when there is no screen
 * with that name, because the design did not declare it. */
bool rib_rmlui_show_screen(const char *id);

/* The cue for an intent. We name it, so we can check it in a test without an
 * audio device, and so a new action is never silent by accident. */
enum rib_menu_sound
{
   RIB_MENU_SOUND_NONE = 0,
   RIB_MENU_SOUND_OK,
   RIB_MENU_SOUND_CANCEL
};

enum rib_menu_sound rib_rmlui_action_sound(int action);

/* The screen whose button the player pressed, which we read when we take
 * RIB_RMLUI_ACTION_SHOW_SCREEN from the queue. */
const char *rib_rmlui_requested_screen(void);

/* The part a slider or toggle just changed, and the value it changed to.
 * Valid until the next such change. */
const char *rib_rmlui_changed_part(void);
float rib_rmlui_changed_fraction(void);
bool rib_rmlui_changed_on(void);

/* The panel for a declared screen. Empty when there is no screen with that
 * name in the design. */
const char *rib_rmlui_screen_panel(const char *id);

/* Draw a slider or a toggle. How they appear is up to the design, and here we
 * only move the fill, the thumb and the on/off class. */
void rib_rmlui_set_slider(const char *id, float fraction, const char *readout);

/* Set a slider the way a drag to that point would, and queue the change.
 * False when the document has no such slider. */
bool rib_rmlui_commit_slider(const char *id, float fraction);

/* One keypress on a slider. The step is part of the control, set when we
 * install the control, and in navigation we do not know which slider moves. */
void rib_rmlui_set_slider_step(const char *id, float step);
bool rib_rmlui_nudge_slider(const char *id, int direction);

/* While a pointer is dragging a slider. The id is valid until the drag ends. */
bool rib_rmlui_slider_drag(const char **id, float *fraction);

/* Focusable parts inside a panel, in document order: sliders, toggles and
 * buttons. We write the ids into storage from the caller. */
int rib_rmlui_focusables(const char *panel, char ids[][64], int capacity);
void rib_rmlui_mark_focused(const char *panel, const char *id);
bool rib_rmlui_part_is_slider(const char *id);

/* The row id, or "prev" / "next", read with LIST_CHOOSE and LIST_PAGE. */
void rib_rmlui_remember_item(const char *id);
const char *rib_rmlui_chosen_item(void);

/* Declared switches. Every element with the list-toggle class is one, and
 * there are no switch names in the bridge. */
void rib_rmlui_wire_toggles(void);
/* Write the word for a switch into `id`-state, and set the `on` class on the
 * switch itself, so a design can draw the two positions differently. */
void rib_rmlui_set_toggle(const char *id, const char *state, bool on);

/* Lock the save slots while something else is on, with the words from the design
 * for the reason. With NULL or empty, the player can use them again. */
void rib_rmlui_guard_slots(const char *label, const char *reason);
bool rib_rmlui_slots_guarded(void);

/* Press the way back from the screen shown. False when there is no back button
 * on it, because the player reached that screen some other way. */
bool rib_rmlui_click_screen_back(void);

/* The button on the pause row that opens a screen, whichever screen the design
 * puts there. Empty when there is none on the pause row. */
const char *rib_rmlui_pause_screen_button(void);

/* Generated lists. The rows are what we drew from the row template in the
 * design, with the list-row class, and we never make a second kind of row. */
void rib_rmlui_wire_lists(void);
int rib_rmlui_visible_row_count(void);
void rib_rmlui_focus_list_row(int index);
/* The controls on a list screen that are not rows: its switch and its back
 * button, in the order we draw them. With the keyboard the player moves past
 * the last row onto these, so a switch is not only for a pointer. */
int rib_rmlui_list_control_count(void);
const char *rib_rmlui_list_control_id(int index);
void rib_rmlui_focus_list_control(int index);
const char *rib_rmlui_list_row_id(int index);
/* -1 when the visible list has a single page. Otherwise the new page index. */
int rib_rmlui_turn_list_page(int delta);
void rib_rmlui_mark_row(const char *id, const char *on, const char *off);

/* Draw a different controller. The markup is a scene for that pad in the
 * export. Call wire_controls afterwards, because the elements with its
 * listeners are gone. */
bool rib_rmlui_set_scene(const char *markup);
int rib_rmlui_hovered_action(void);
void rib_rmlui_clear_intents(void);
bool rib_rmlui_element_center(const char *id, int *x, int *y);
bool rib_rmlui_element_disabled(const char *id);
bool rib_rmlui_reload_if_changed(void);

/* The game has started. Run the overlays declared in the design.
 *
 * We draw them over the running game, so the core is not paused and the
 * player's controller mapping stays active. */
void rib_rmlui_begin_overlays(void);

/* Whether we still want frames for the menu driver while the menu is closed.
 * In every video driver we skip the menu while it is closed, so without this
 * we would never draw an overlay. */
bool rib_rmlui_overlays_drawing(void);
bool rib_rmlui_consume_menu_toggle(void *userdata);
void rib_rmlui_notify_state_task(const char *path, int slot,
      bool is_save, bool success);

#ifdef __cplusplus
}
#endif

#endif
