#ifndef RMLUI_BRIDGE_H
#define RMLUI_BRIDGE_H

#include <stdbool.h>
#include <string.h>

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
   RIB_RMLUI_ACTION_CONTROL_LAST = RIB_RMLUI_ACTION_CONTROL_FIRST + 47
};

/* How many control ids there are in the player, and each id in the order of
 * the console declaration. The code is in rmlui.c, where we read the list
 * from the exported controls configuration. On purpose, there are no control
 * names in the bridge. */
/* Attach listeners to control elements. Call it after we know the control
 * list. We load the document before that, so at load time there is nothing
 * to attach to. */
void rib_rmlui_wire_controls(void);

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
void rib_rmlui_set_splash(bool visible, float opacity);
void rib_rmlui_pointer_move(int x, int y);
void rib_rmlui_pointer_button(bool down);
void rib_rmlui_pointer_leave(void);
int rib_rmlui_take_action(void);
int rib_rmlui_hovered_action(void);
void rib_rmlui_clear_intents(void);
bool rib_rmlui_element_center(const char *id, int *x, int *y);
bool rib_rmlui_element_disabled(const char *id);
bool rib_rmlui_reload_if_changed(void);
void rib_rmlui_begin_splash(bool keep_menu_open);
bool rib_rmlui_splash_active(void);
bool rib_rmlui_consume_menu_toggle(void *userdata);
void rib_rmlui_notify_state_task(const char *path, int slot,
      bool is_save, bool success);

#ifdef __cplusplus
}
#endif

#endif
