#include <stdlib.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include <new>
#include <stdint.h>
#include <retro_miscellaneous.h>
#include <file/file_path.h>
#include <file/config_file.h>
#include <streams/file_stream.h>
#include <string/stdstring.h>
#include "../../../verbosity.h"
#include "../rmlui_bridge.h"
#include "menu_api.h"
#include "files.h"
#include "host.h"
#include "declarations.h"
#include "overlays.hpp"
#include "script.hpp"
#include "shaders.hpp"
#include "discs.hpp"
#include "settings.hpp"
#include "controls.hpp"
#include "slots.hpp"

#ifndef RIB_RMLUI_DEFAULT_ASSETS
#define RIB_RMLUI_DEFAULT_ASSETS "."
#endif

typedef struct rib_rmlui_menu
{
   bool initialized;
   bool overlay_mode;
   rib::Overlays overlays;
   rib::Script script;
   rib::Shaders shaders;
   rib::Discs discs;
   rib::Toggles toggles;
   rib::Volume volume;
   rib::Focus focus;
   rib::Screens& screens = rib_rmlui_screens();
   rib::Controls controls{focus, screens};
   bool pointer_pressed;
   bool transfer_pending;
   bool transfer_is_save;
   rib::Slots *slots;
   int transfer_slot;
   char transfer_path[PATH_MAX_LENGTH];
} rib_rmlui_menu_t;

/* The public runloop callback can come before we allocate the menu. Keep only
 * that pending request here, and the overlay timeline in the menu itself. */
static bool pending_overlay_start;
static rib_rmlui_menu_t *rib_rmlui_active_menu;

static void rib_focus_list(rib_rmlui_menu_t *menu, int index);
int rib_menu_key(void *data, enum rib_key action);

void rib_rmlui_begin_overlays(void)
{
   if (rib_rmlui_active_menu)
      rib_rmlui_active_menu->overlays.begin();
   else
   {
      pending_overlay_start = true;
      rib_host_overlay_frames(true);
   }
}

bool rib_rmlui_overlays_drawing(void)
{
   return pending_overlay_start ||
         (rib_rmlui_active_menu && rib_rmlui_active_menu->overlays.drawing());
}

static const char *rib_absolute_data_dir(void)
{
   const char *data = getenv("ROMINABOX_DATA_DIR");

   if (!data || data[0] != '/')
      return NULL;
   return data;
}

/* We load the declarations once. These assignments set the starting state of
 * each feature. There is no document or input code in the loader. */
static void rib_rmlui_load_design(rib_rmlui_menu_t *menu, const char *assets)
{
   rib_design_declarations *loaded = rib_load_design(assets);
   const rib_design_data *design = rib_design_get(loaded);
   size_t index;
   rib_rmlui_clear_screens();
   menu->discs.configure(*design);
   for (index = 0; index < design->screen_count; ++index)
   {
      const rib_screen_declaration *screen = &design->screens[index];
      rib_rmlui_declare_screen(screen->id, screen->panel, screen->heading,
            screen->footer, screen->button);
   }
   menu->toggles.load(*design, rib_absolute_data_dir());
   menu->overlays.load(*design);
   menu->controls.configure_binds(*design);
   rib_design_free(loaded);
}

/* The buttons of the pause row, in document order, without the disabled
 * ones.
 *
 * The pause row contains what the design and the export put in it. With
 * Options on, Options is the fourth button, and a game without Options has
 * one button fewer.
 *
 * We leave disabled buttons out of the list and do not skip them when the
 * focus reaches them, because LOAD is disabled until there is a save.
 */
static int rib_pause_row(char ids[][64], int capacity)
{
   char all[16][64];
   int found = rib_rmlui_focusables("pause-panel", all, 16);
   int count = 0;
   int index;

   for (index = 0; index < found && count < capacity; ++index)
   {
      if (rib_rmlui_element_disabled(all[index]))
         continue;
      strlcpy(ids[count], all[index], 64);
      ++count;
   }
   return count;
}

/* The position of the focused button in that row now. The row can change while
 * a button is focused, for example LOAD becomes usable once a slot has a save,
 * so we look it up on every move and do not store it. */
static int rib_pause_row_index(const char ids[][64], int count)
{
   const char *focused = rib_rmlui_focused_element();
   int index;

   if (focused && *focused)
      for (index = 0; index < count; ++index)
         if (string_is_equal(ids[index], focused))
            return index;
   return 0;
}

static void rib_pause_focus_row(rib_rmlui_menu_t *menu, int index,
      bool direction_up)
{
   char ids[16][64];
   const int count = rib_pause_row(ids, 16);

   if (count <= 0)
      return;
   index = ((index % count) + count) % count;
   menu->focus.pause_row(index);
   rib_rmlui_focus_element(ids[index]);
#ifdef HAVE_AUDIOMIXER
   rib_host_scroll_sound(direction_up);
#endif
}

static bool rib_rmlui_focus_is_slot(const rib::Event& focused)
{
   return focused.kind == RIB_RMLUI_ACTION_SELECT_SLOT && rib::valid_slot(focused.slot);
}

static int rib_rmlui_focus_slot(const rib::Event& focused)
{
   return focused.slot;
}

static void rib_rmlui_select_slot(rib_rmlui_menu_t *menu, int slot)
{
   if (!menu || !rib::valid_slot(slot))
      return;
   rib_rmlui_set_selected_slot(slot);
}

static bool rib_rmlui_load_is_available(const rib_rmlui_menu_t *menu);

static void rib_rmlui_focus(rib_rmlui_menu_t *menu, rib::Event focused,
      bool direction_up)
{
   bool changed;

   if (!menu)
      return;
   if (focused.kind == RIB_RMLUI_ACTION_LOAD &&
       !rib_rmlui_load_is_available(menu))
      focused = direction_up ? RIB_RMLUI_ACTION_SAVE :
            RIB_RMLUI_ACTION_CONTROLS;
   changed = !menu->focus.pause_action().same_target(focused);
   menu->focus.pause_action(focused);
   /* Focusing an action or a slot here moves the focus off the row. We track
    * the row by id, not by this enum. */
   menu->focus.pause_row(-1);
   rib_rmlui_set_focused(focused);
   if (rib_rmlui_focus_is_slot(focused))
      rib_rmlui_select_slot(menu, rib_rmlui_focus_slot(focused));
#ifdef HAVE_AUDIOMIXER
   if (changed)
      rib_host_scroll_sound(direction_up);
#endif
}

static bool rib_rmlui_load_is_available(const rib_rmlui_menu_t *menu)
{
   return menu && rib_host_slot_occupied(menu->slots->selected());
}

static void rib_rmlui_refresh_slots(void)
{
   int slot;
   for (slot = 1; slot <= rib::kSlotCount; ++slot)
   {
      char thumbnail_path[PATH_MAX_LENGTH] = {0};
      bool occupied = rib_host_slot_occupied(slot);
      rib_host_thumbnail(slot, thumbnail_path, sizeof(thumbnail_path));
      rib_rmlui_set_game_aspect(rib_host_game_aspect());
      rib_rmlui_set_slot_state(slot, occupied, thumbnail_path);
   }
}

static bool rib_rmlui_begin_transfer(rib_rmlui_menu_t *menu, bool is_save)
{
   if (!menu || menu->transfer_pending)
      return false;

   menu->transfer_path[0] = '\0';
   if (!rib_host_state_path(menu->slots->selected(), menu->transfer_path,
         sizeof(menu->transfer_path)))
      menu->transfer_path[0] = '\0';
   menu->transfer_is_save = is_save;
   menu->transfer_slot = menu->slots->selected();
   menu->transfer_pending = true;
   return true;
}

void rib_rmlui_notify_state_task(const char *path, int slot,
      bool is_save, bool success)
{
   rib_rmlui_menu_t *menu = rib_rmlui_active_menu;
   char status[64];

   if (!rib_rmlui_state_task_matches(
         menu && menu->transfer_pending,
         menu && menu->transfer_is_save,
         menu ? menu->transfer_path : NULL,
         menu ? menu->transfer_slot : -1,
         path, slot, is_save))
      return;

   menu->transfer_pending = false;
   if (success)
      snprintf(status, sizeof(status),
            is_save ? "SLOT %d SAVED" : "SLOT %d LOADED",
            menu->transfer_slot);
   else
      snprintf(status, sizeof(status),
            is_save ? "SAVE FAILED" : "LOAD FAILED");
   rib_rmlui_set_status(status);
}

static void rib_rmlui_reset_interaction(rib_rmlui_menu_t *menu, bool opening)
{
   if (!menu)
      return;
   if (menu->controls.capture_active)
      menu->controls.cancel_capture(NULL);
   menu->screens.remember("pause");
   rib_focus_list(menu, 0);
   menu->pointer_pressed = false;
   menu->controls.capture_ignore_pointer = false;
   menu->focus.pause_action(RIB_RMLUI_ACTION_RESUME);
   /* When the menu opens, we focus the first button of the row, not a slot. */
   menu->focus.pause_row(0);
   rib_rmlui_clear_intents();
   rib_rmlui_pointer_leave();
   if (opening)
   {
      rib_rmlui_show_controls(false);
      rib_rmlui_set_focused(menu->focus.pause_action());
      rib_rmlui_set_footer_hint("ESC  CONTINUE");
   }
}

void rib_menu_toggle(void *userdata, bool on)
{
   rib_rmlui_reset_interaction((rib_rmlui_menu_t*)userdata, on);
}

bool rib_menu_consume_toggle(void *userdata)
{
   rib_rmlui_menu_t *menu = (rib_rmlui_menu_t*)userdata;
   return menu && rib_rmlui_toggle_stays_in_menu(
         menu->screens.controls_visible() || !string_is_equal(menu->screens.current(), "pause"),
         menu->controls.capture_active);
}

/* A step that changed the level. We play the up or down wav from the sound
 * pack, the same cue as for a focus move, and not the confirm cue. */
void rib_rmlui_play_move_sound(int direction)
{
#ifdef HAVE_AUDIOMIXER
   if (direction != 0)
      rib_host_scroll_sound(direction > 0);
#else
   (void)direction;
#endif
}

static void rib_rmlui_play_action_sound(int action)
{
#ifdef HAVE_AUDIOMIXER
   switch (rib_rmlui_action_sound(action))
   {
      case RIB_MENU_SOUND_OK:
         rib_host_ok_sound();
         break;
      case RIB_MENU_SOUND_CANCEL:
         rib_host_cancel_sound();
         break;
      case RIB_MENU_SOUND_NONE:
         break;
   }
#endif
}

/* The position of the keyboard focus on a list screen. The rows come first, then
 * the other controls of the screen, so moving down past the last row reaches the
 * switch and BACK. A player with a pad could not use a switch that only a
 * pointer can reach. */
static void rib_rmlui_focus_list(rib_rmlui_menu_t *menu)
{
   const int rows = rib_rmlui_visible_row_count();

   if (!menu)
      return;
   if (menu->focus.position(rib::FocusRegion::List) < rows)
   {
      rib_rmlui_focus_list_row(menu->focus.position(rib::FocusRegion::List));
      rib_rmlui_focus_list_control(-1);
      return;
   }
   rib_rmlui_focus_list_row(-1);
   rib_rmlui_focus_list_control(menu->focus.position(rib::FocusRegion::List) - rows);
}

/* The only place where we write list_focus. We call it for the keys and for
 * the pointer, in every list, the shader list included. */
static void rib_focus_list(rib_rmlui_menu_t *menu, int index)
{
   if (!menu || index < 0)
      return;
   menu->focus.position(rib::FocusRegion::List, index);
   rib_rmlui_focus_list(menu);
}

static void rib_rmlui_perform_action(rib_rmlui_menu_t *menu, const rib::Event& event)
{
   const auto action = event.kind;
   char status[64];
   int control_index;

   if (!menu)
      return;

   if (menu->controls.capture_active &&
       action != RIB_RMLUI_ACTION_CONTROLS_CANCEL &&
       action != RIB_RMLUI_ACTION_CONTROLS_BACK)
      return;

   if (action == RIB_RMLUI_ACTION_DEVICE_PICKER_TOGGLE)
   {
      menu->controls.toggle_picker();
      return;
   }
   if (action == RIB_RMLUI_ACTION_SLIDER)
   {
      rib_rmlui_play_action_sound(action);
      if (string_is_equal(event.id.c_str(), RIB_VOLUME_SLIDER_ID))
         menu->volume.set(
               rib_volume_db_from_fraction(event.fraction), true);
      return;
   }
   if (action == RIB_RMLUI_ACTION_LIST_CHOOSE)
   {
      const char *id = event.id.c_str();
      int row;

      if (menu->discs.choose(menu->screens.current(), id))
      {
         rib_rmlui_play_action_sound(action);
         menu->discs.sync();
      }
      else if (menu->shaders.apply(id, getenv("ROMINABOX_RML_ASSETS"),
               rib_absolute_data_dir()))
         rib_rmlui_play_action_sound(action);
      for (row = 0; row < rib_rmlui_visible_row_count(); ++row)
         if (string_is_equal(rib_rmlui_list_row_id(row), id))
         {
            rib_focus_list(menu, row);
            break;
         }
      return;
   }
   if (action == RIB_RMLUI_ACTION_LIST_PAGE)
   {
      const char *which = event.id.c_str();
      int delta = which && string_is_equal(which, "prev") ? -1 : 1;

      rib_rmlui_play_action_sound(action);
      if (rib_rmlui_turn_list_page(delta) >= 0)
         rib_focus_list(menu, 0);
      return;
   }
   if (action == RIB_RMLUI_ACTION_PART_TOGGLE)
   {
      rib_rmlui_play_action_sound(action);
      return;
   }
   if (action == RIB_RMLUI_ACTION_TOGGLE)
   {
      const char *id = event.id.c_str();
      rib_rmlui_play_action_sound(action);
      menu->toggles.toggle(id, rib_absolute_data_dir());
      return;
   }
   if (action == RIB_RMLUI_ACTION_SHOW_SCREEN)
   {
      /* We capture and navigate on the active screen, with the id from the event. */
      const char *wanted = event.id.c_str();
      char screen_id[32];

      screen_id[0] = '\0';
      if (wanted && *wanted)
         strlcpy(screen_id, wanted, sizeof(screen_id));
      /* Pressing the button in the column opens the circle. With more than one
       * image, we make it open the list instead, because a second button would
       * move the column, and hiding the only button would leave a gap. */
      menu->discs.redirect(screen_id, sizeof(screen_id));
      if (screen_id[0] && rib_rmlui_show_screen(screen_id))
      {
         /* The footer and the heading are in the design, with the screen.
          * Here we keep only the case of the controls screen, where capture
          * and navigation work differently. For any other screen there is
          * nothing to add here. */
         menu->screens.remember(screen_id);
         rib_focus_list(menu, 0);
         if (menu->screens.controls_visible())
         {
            menu->controls.focus(menu->controls.first());
            rib_rmlui_set_controls_status("SELECT A CONTROL TO REBIND");
            menu->controls.refresh();
         }
         else if (menu->controls.capture_active)
            menu->controls.cancel_capture("BINDING UNCHANGED");
         else if (!string_is_equal(screen_id, "pause"))
         {
            char ids[16][64];
            const char *panel = rib_rmlui_screen_panel(menu->screens.current());
            int count = rib_rmlui_focusables(panel, ids, 16);
            bool slider = false;
            int index;

            for (index = 0; index < count; ++index)
               if (rib_rmlui_part_is_slider(ids[index]))
                  slider = true;
            /* A slider is the first thing a keyboard should land on: left and
             * right move it. A list with no slider focuses its first row, and
             * then the screen's own controls past that. */
            if (slider)
            {
               menu->focus.position(rib::FocusRegion::Parts, 0);
               rib_rmlui_mark_focused(panel, ids[0]);
            }
            else
               rib_rmlui_focus_list(menu);
         }
         /* We measure the slider from the box of its track. While the panel
          * is hidden that width is zero, so a paint leaves the thumb where
          * the stylesheet put it, at the quiet end. Paint it again now that
          * the screen is shown. */
         menu->volume.paint();
         menu->shaders.show_running();
      }
      rib_rmlui_play_action_sound(action);
      return;
   }

   if (action == RIB_RMLUI_ACTION_DEVICE_PICKER_CHOOSE)
   {
      menu->controls.choose_device(event.id.c_str());
      return;
   }

   if (action == RIB_RMLUI_ACTION_CONTROL)
   {
      control_index = menu->controls.index_of(event.id.c_str());
      if (menu->controls.active(control_index))
      {
         rib_rmlui_play_action_sound(action);
         menu->controls.focus(rib::FocusTarget::item(control_index));
         menu->controls.start_capture(control_index);
      }
      return;
   }

   if ((action == RIB_RMLUI_ACTION_SAVE ||
            action == RIB_RMLUI_ACTION_LOAD) &&
         (menu->transfer_pending || rib_rmlui_slots_guarded()))
      return;
   if (rib_rmlui_slots_guarded() && rib_rmlui_focus_is_slot(event))
      return;

   if (action == RIB_RMLUI_ACTION_LOAD && !rib_rmlui_load_is_available(menu))
      return;
   rib_rmlui_play_action_sound(action);

   switch (action)
   {
      case RIB_RMLUI_ACTION_SAVE:
         if (!rib_rmlui_begin_transfer(menu, true))
            return;
         rib_host_select_state_slot(menu->slots->selected());
         snprintf(status, sizeof(status), "SAVING SLOT %d...",
               menu->slots->selected());
         rib_rmlui_set_status(status);
         if (!rib_host_save_state() &&
               menu->transfer_pending)
            rib_rmlui_notify_state_task(menu->transfer_path,
                  menu->transfer_slot, true, false);
         break;
      case RIB_RMLUI_ACTION_LOAD:
         if (!rib_rmlui_load_is_actionable(rib_rmlui_load_is_available(menu)))
            return;
         if (!rib_rmlui_begin_transfer(menu, false))
            return;
         rib_host_select_state_slot(menu->slots->selected());
         snprintf(status, sizeof(status), "LOADING SLOT %d...",
               menu->slots->selected());
         rib_rmlui_set_status(status);
         if (!rib_host_load_state() &&
               menu->transfer_pending)
            rib_rmlui_notify_state_task(menu->transfer_path,
                  menu->transfer_slot, false, false);
         break;
      case RIB_RMLUI_ACTION_CONTROLS:
      {
         /* Open the screen on the pause row in the design, through its button.
          * We use this path for both the keyboard and the pointer. */
         const char *button = rib_rmlui_pause_screen_button();

         if (button && *button)
         {
            rib_rmlui_click_element(button);
            return;
         }
      }
         menu->screens.remember("controls");
         menu->controls.focus(menu->controls.first());
         /* The heading and the footer are in the design, with the
          * screen. */
         rib_rmlui_show_screen("controls");
         rib_rmlui_set_controls_status("SELECT A CONTROL TO REBIND");
         menu->controls.refresh();
         break;
      case RIB_RMLUI_ACTION_CONTROLS_BACK:
         if (menu->controls.capture_active)
            menu->controls.cancel_capture("BINDING UNCHANGED");
         menu->screens.remember("pause");
         menu->focus.pause_action(RIB_RMLUI_ACTION_CONTROLS);
         rib_rmlui_show_screen("pause");
         rib_rmlui_set_focused(menu->focus.pause_action());
         break;
      case RIB_RMLUI_ACTION_CONTROLS_CANCEL:
         menu->controls.cancel_capture("BINDING UNCHANGED");
         break;
      case RIB_RMLUI_ACTION_CONTROLS_RESET:
         menu->controls.reset_defaults();
         break;
      case RIB_RMLUI_ACTION_RESUME:
         rib_host_resume();
         break;
      case RIB_RMLUI_ACTION_QUIT:
         rib_host_quit();
         break;
      case RIB_RMLUI_ACTION_SELECT_SLOT:
         rib_rmlui_focus(menu, event, false);
         break;
      default:
         break;
   }
}

void *rib_menu_create(void)
{
   rib_rmlui_menu_t *menu = new (std::nothrow) rib_rmlui_menu_t{};
   if (!menu)
      return nullptr;
   menu->slots = &rib_rmlui_bind_state(menu->focus, menu->controls.catalog);
   menu->slots->set_selected_slot(1);
   menu->focus.pause_action(RIB_RMLUI_ACTION_RESUME);
   menu->focus.pause_row(0);
   menu->screens.remember("pause");
   rib_rmlui_active_menu = menu;
   if (pending_overlay_start)
   {
      pending_overlay_start = false;
      menu->overlays.begin();
   }
   return menu;
}

void rib_menu_destroy(void *data)
{
   rib_rmlui_menu_t *menu = (rib_rmlui_menu_t*)data;
   if (rib_rmlui_active_menu == data)
      rib_rmlui_active_menu = NULL;
   if (menu && menu->controls.capture_active)
      menu->controls.cancel_capture(NULL);
   rib_rmlui_shutdown();
   pending_overlay_start = false;
   delete menu;
   /* We leave the small userdata wrapper of the C adapter to menu_driver_ctl. */
}

void rib_menu_context_destroy(void *data)
{
   rib_rmlui_menu_t *menu = (rib_rmlui_menu_t*)data;
   if (menu && menu->controls.capture_active)
      menu->controls.cancel_capture(NULL);
   rib_rmlui_shutdown();
   if (menu)
      menu->initialized = false;
}

void rib_menu_context_reset(void *data)
{
   rib_rmlui_menu_t *menu = (rib_rmlui_menu_t*)data;
   if (menu)
      menu->initialized = false;
   /* With a new video driver the menu is switched off, so for anything still
    * drawn over the game we ask for frames again. This happens, for example,
    * when the player goes fullscreen during an overlay. */
   if (menu && menu->overlays.drawing())
      rib_host_overlay_frames(true);
}

void rib_menu_frame(void *data, int width, int height)
{
   rib_rmlui_menu_t *menu = (rib_rmlui_menu_t*)data;
   rib_pointer pointer;
   const char *asset_directory = getenv("ROMINABOX_RML_ASSETS");
   const char *data_directory = rib_absolute_data_dir();

   if (!menu)
      return;

   if (!menu->initialized)
   {
      if (!asset_directory || !*asset_directory)
         asset_directory = RIB_RMLUI_DEFAULT_ASSETS;
      menu->initialized = rib_rmlui_init(asset_directory,
            width, height,
            rib_host_core_gl_context(), menu->focus, menu->controls.catalog);
      if (!menu->initialized)
      {
         RARCH_ERR("[RmlUi] Failed to initialize menu from %s.\n",
               asset_directory);
         menu->overlays.stop();
         rib_host_overlay_frames(false);
         return;
      }
      /* Read the screens and overlays in the design before we show any. */
      rib_rmlui_load_design(menu, asset_directory);
      menu->shaders.load(asset_directory);
      rib_rmlui_set_selected_slot(menu->slots->selected());
      rib_rmlui_set_focused(menu->focus.pause_action());
      rib_rmlui_refresh_slots();
      rib_rmlui_show_controls(false);
      rib_rmlui_set_footer_hint("ESC  CONTINUE");
      if (!menu->controls.loaded)
      {
         char defaults_path[PATH_MAX_LENGTH];
         snprintf(defaults_path, sizeof(defaults_path),
               "%s/controls-defaults.cfg", asset_directory);
         if (!menu->controls.load_file(defaults_path, true))
            RARCH_WARN("[RmlUi] Controls defaults not found at %s.\n",
                  defaults_path);
         if (data_directory && *data_directory)
         {
            snprintf(menu->controls.path, sizeof(menu->controls.path),
                  "%s/controls.cfg", data_directory);
            menu->volume.configure_path(data_directory);
            menu->controls.load_file(menu->controls.path, false);
         }
         menu->controls.loaded = true;
      }
      menu->controls.focus(menu->controls.first());
      menu->controls.refresh();
      menu->volume.initialize();
      /* After the slots, so the lock from a switch replaces the slot count. */
      menu->toggles.apply();
      RARCH_LOG("[RmlUi] Loaded menu from %s.\n", asset_directory);
   }

   {
      const bool menu_alive = rib_host_menu_open();

      /* When we draw this document while the menu is closed, the menu itself
       * is not on screen, and we state that on the document for the design.
       * We set it here and not with the overlays, because we may still draw
       * the document after the last overlay has gone. */
      if (menu->overlay_mode != !menu_alive)
      {
         menu->overlay_mode = !menu_alive;
         rib_rmlui_set_overlay_mode(menu->overlay_mode);
      }

      if (!menu_alive)
      {
         /* The game is running and the player is playing it with the
          * controller. Nothing below applies now: no pointer, no queued
          * actions and no capture. We have a frame in this driver only to
          * draw over the game.
          *
          * We run the script first, because from it we learn whether we still
          * want frames after the overlays are done, and we can learn that only
          * after asking it. */
         menu->script.run(menu, {menu->screens.current(), menu->transfer_pending,
               menu->controls.capture_active, menu->controls.profile_id});
         menu->overlays.update(menu->script.wants_frames());
         rib_rmlui_render(width, height);
         return;
      }
      menu->overlays.update(menu->script.wants_frames());
   }

   pointer = rib_host_pointer();
   {
      bool pointer_pressed =
            pointer.pressed;

      rib_rmlui_pointer_move(pointer.x, pointer.y);
      menu->script.restore_hover();
      rib_rmlui_pointer_button(pointer_pressed);

      if (menu->controls.capture_active && pointer_pressed && !menu->pointer_pressed &&
            (rib_rmlui_hovered_event().kind == RIB_RMLUI_ACTION_CONTROLS_CANCEL ||
             rib_rmlui_hovered_event().kind == RIB_RMLUI_ACTION_CONTROLS_BACK))
         menu->controls.capture_ignore_pointer = true;
      if (menu->controls.capture_ignore_pointer && !pointer_pressed)
         menu->controls.capture_ignore_pointer = false;
      menu->pointer_pressed = pointer_pressed;
   }

   {
      const char *drag_id = NULL;
      float drag_fraction = 0.0f;
      if (rib_rmlui_slider_drag(&drag_id, &drag_fraction) && drag_id
            && string_is_equal(drag_id, RIB_VOLUME_SLIDER_ID))
         menu->volume.set(
               rib_volume_db_from_fraction(drag_fraction), false);
      /* In the frame where a screen appears, the track may not be laid out yet,
       * and a fill set from that width stays too short after the track grows.
       * We paint again on the next frames, with the width the player sees. */
      else
         menu->volume.paint();
   }

   /* Before we empty the queue, so we handle a scripted click in this frame,
    * in the same loop as a click from the player. We put back a hover from
    * the script after the click, because the pointer move above followed the
    * mouse and would have removed the hover. The disc entry starts hidden.
    * Fill it before the click from the script, or the click goes to a button
    * that is still display:none in the document. */
   menu->discs.sync();
   menu->script.run(menu, {menu->screens.current(), menu->transfer_pending,
               menu->controls.capture_active, menu->controls.profile_id});
   menu->script.restore_hover();
   /* After we put the pointer back for the script, so a hovered row is the
    * focused row before the click in this frame. We play no scroll sound,
    * because the pointer did not move by a step. */
   if (!menu->controls.capture_active)
      rib_focus_list(menu, rib_rmlui_hovered_list_row());

   for (;;)
   {
      auto next_action = rib_rmlui_take_event();
      if (next_action.kind == RIB_RMLUI_ACTION_NONE)
         break;
      rib_rmlui_perform_action(menu, next_action);
   }

   menu->controls.poll_capture();

   rib_rmlui_reload_if_changed();
   rib_rmlui_refresh_slots();
   menu->controls.update_binds(pointer.x, pointer.y,
         !menu->script.wants_frames(),
         !menu->script.wants_frames() || menu->script.has_hover());
   rib_rmlui_render(width, height);

}

/* A screen other than Pause and Controls, whose navigation we leave as it
 * was. On any other screen, the player moves through the parts of the panel in
 * the design, such as a slider, a toggle or a button, in document order. */
static int rib_part_navigate(rib_rmlui_menu_t *menu, enum rib_key action)
{
   char ids[16][64];
   const char *panel = rib_rmlui_screen_panel(menu->screens.current());
   int count = rib_rmlui_focusables(panel, ids, 16);

   if (count <= 0)
      return 0;
   if (menu->focus.position(rib::FocusRegion::Parts) < 0 || menu->focus.position(rib::FocusRegion::Parts) >= count)
      menu->focus.position(rib::FocusRegion::Parts, 0);

   switch (action)
   {
      case RIB_KEY_UP:
         menu->focus.position(rib::FocusRegion::Parts, rib::Focus::ring(menu->focus.position(rib::FocusRegion::Parts), count, -1));
         rib_rmlui_mark_focused(panel, ids[menu->focus.position(rib::FocusRegion::Parts)]);
#ifdef HAVE_AUDIOMIXER
         rib_host_scroll_sound(true);
#endif
         return 0;
      case RIB_KEY_DOWN:
         menu->focus.position(rib::FocusRegion::Parts, rib::Focus::ring(menu->focus.position(rib::FocusRegion::Parts), count, 1));
         rib_rmlui_mark_focused(panel, ids[menu->focus.position(rib::FocusRegion::Parts)]);
#ifdef HAVE_AUDIOMIXER
         rib_host_scroll_sound(false);
#endif
         return 0;
      case RIB_KEY_LEFT:
      case RIB_KEY_RIGHT:
         if (rib_rmlui_part_is_slider(ids[menu->focus.position(rib::FocusRegion::Parts)]))
            rib_rmlui_nudge_slider(ids[menu->focus.position(rib::FocusRegion::Parts)],
                  action == RIB_KEY_RIGHT ? 1 : -1);
         return 0;
      case RIB_KEY_OK:
      case RIB_KEY_SELECT:
         rib_rmlui_click_element(ids[menu->focus.position(rib::FocusRegion::Parts)]);
         return 0;
      case RIB_KEY_CANCEL:
      case RIB_KEY_RESUME:
      case RIB_KEY_TOGGLE:
         rib_rmlui_play_action_sound(RIB_RMLUI_ACTION_CONTROLS_BACK);
         menu->screens.remember("pause");
         rib_rmlui_show_screen("pause");
         rib_rmlui_mark_focused(panel, NULL);
         return 0;
      default:
         return 0;
   }
}

int rib_menu_key(void *data, enum rib_key action)
{
   rib_rmlui_menu_t *menu = (rib_rmlui_menu_t*)data;

   if (!menu)
      return 0;

   if (!string_is_equal(menu->screens.current(), "pause") && !menu->screens.controls_visible())
   {
      char ids[16][64];
      const char *panel = rib_rmlui_screen_panel(menu->screens.current());
      int parts = rib_rmlui_focusables(panel, ids, 16);
      int rows = rib_rmlui_visible_row_count();
      bool slider = false;
      int part;

      for (part = 0; part < parts; ++part)
         if (rib_rmlui_part_is_slider(ids[part]))
            slider = true;
      /* Left and right move a slider. They page a list only when the panel
       * has no slider, which is how a shader list turns its pages. */
      if (slider)
         return rib_part_navigate(menu, action);
      if (rows <= 0)
         return rib_part_navigate(menu, action);

      const int stops = rows + rib_rmlui_list_control_count();

      switch (action)
      {
         case RIB_KEY_UP:
            if (stops > 0)
            {
               rib_focus_list(menu, rib::Focus::ring(menu->focus.position(rib::FocusRegion::List), stops, -1));
#ifdef HAVE_AUDIOMIXER
               rib_host_scroll_sound(true);
#endif
            }
            return 0;
         case RIB_KEY_DOWN:
            if (stops > 0)
            {
               rib_focus_list(menu, rib::Focus::ring(menu->focus.position(rib::FocusRegion::List), stops, 1));
#ifdef HAVE_AUDIOMIXER
               rib_host_scroll_sound(false);
#endif
            }
            return 0;
         case RIB_KEY_LEFT:
            if (rib_rmlui_turn_list_page(-1) >= 0)
            {
               rib_focus_list(menu, 0);
               rib_rmlui_play_action_sound(RIB_RMLUI_ACTION_LIST_PAGE);
            }
            return 0;
         case RIB_KEY_RIGHT:
            if (rib_rmlui_turn_list_page(1) >= 0)
            {
               rib_focus_list(menu, 0);
               rib_rmlui_play_action_sound(RIB_RMLUI_ACTION_LIST_PAGE);
            }
            return 0;
         case RIB_KEY_OK:
         case RIB_KEY_SELECT:
            if (menu->focus.position(rib::FocusRegion::List) >= rows)
            {
               /* Through the listener on the element, the same path as for a
                * pointer, which already has the code for the switch and BACK. */
               rib_rmlui_click_element(
                     rib_rmlui_list_control_id(menu->focus.position(rib::FocusRegion::List) - rows));
            }
            else if (rows > 0)
            {
               rib_rmlui_perform_action(menu, {RIB_RMLUI_ACTION_LIST_CHOOSE,
                     rib_rmlui_list_row_id(menu->focus.position(rib::FocusRegion::List))});
            }
            return 0;
         case RIB_KEY_CANCEL:
         case RIB_KEY_RESUME:
         case RIB_KEY_TOGGLE:
            /* We leave this screen through its back button, so pressing Escape
             * goes to the same place as BACK, including back to Options from a
             * screen opened from Options. */
            if (!rib_rmlui_click_screen_back())
               rib_rmlui_perform_action(menu, rib_rmlui_map_menu_toggle(
                     true, false));
            return 0;
         default:
            return 0;
      }
   }

   if (menu->screens.controls_visible())
   {
      if (menu->controls.capture_active)
      {
         if (action == RIB_KEY_CANCEL || action == RIB_KEY_RESUME ||
             action == RIB_KEY_TOGGLE)
            rib_rmlui_perform_action(menu, RIB_RMLUI_ACTION_CONTROLS_CANCEL);
         return 0;
      }

      switch (action)
      {
         case RIB_KEY_UP:
         case RIB_KEY_LEFT:
#ifdef HAVE_AUDIOMIXER
            rib_host_scroll_sound(true);
#endif
            menu->controls.focus(menu->controls.step(-1));
            return 0;
         case RIB_KEY_DOWN:
         case RIB_KEY_RIGHT:
#ifdef HAVE_AUDIOMIXER
            rib_host_scroll_sound(false);
#endif
            menu->controls.focus(menu->controls.step(1));
            return 0;
         case RIB_KEY_OK:
         case RIB_KEY_SELECT:
            if (menu->focus.target(rib::FocusRegion::Controls).kind == rib::FocusTarget::Kind::Item)
               menu->controls.start_capture(menu->focus.position(rib::FocusRegion::Controls));
            else if (menu->focus.target(rib::FocusRegion::Controls).kind == rib::FocusTarget::Kind::Reset)
               rib_rmlui_perform_action(menu, RIB_RMLUI_ACTION_CONTROLS_RESET);
            else
               rib_rmlui_perform_action(menu, RIB_RMLUI_ACTION_CONTROLS_BACK);
            return 0;
         case RIB_KEY_START:
            rib_rmlui_perform_action(menu, RIB_RMLUI_ACTION_CONTROLS_RESET);
            return 0;
         case RIB_KEY_CANCEL:
         case RIB_KEY_RESUME:
         case RIB_KEY_TOGGLE:
            /* We leave this screen through its back button, so pressing Escape
             * goes to the same place as BACK, including back to Options from a
             * screen opened from Options. */
            if (!rib_rmlui_click_screen_back())
               rib_rmlui_perform_action(menu, rib_rmlui_map_menu_toggle(
                     true, false));
            return 0;
         default:
            return 0;
      }
   }

   if (menu->screens.current()[0] && !string_is_equal(menu->screens.current(), "pause"))
      return rib_part_navigate(menu, action);

   switch (action)
   {
      case RIB_KEY_UP:
         if (menu->focus.pause_row() < 0)
         {
            int slot = rib_rmlui_focus_slot(menu->focus.pause_action());
            if (slot > 3)
               rib_rmlui_focus(menu, rib::Event::select_slot(slot - 3), true);
            else
            {
               /* From the top row of slots to the buttons, in the column of
                * the slot. When the row has fewer than three buttons, we
                * focus its last button. */
               char ids[16][64];
               const int count = rib_pause_row(ids, 16);
               rib_pause_focus_row(menu,
                     slot - 1 < count ? slot - 1 : count - 1, true);
            }
         }
         else
         {
            const int column = menu->focus.pause_row() > 2 ? 2 : menu->focus.pause_row();
            menu->focus.pause_row(-1);
            rib_rmlui_focus(menu,
                  rib::Event::select_slot(4 + column), true);
         }
         return 0;
      case RIB_KEY_DOWN:
         if (menu->focus.pause_row() < 0)
         {
            int slot = rib_rmlui_focus_slot(menu->focus.pause_action());
            if (slot <= 3)
               rib_rmlui_focus(menu, rib::Event::select_slot(slot + 3), false);
            else
            {
               char ids[16][64];
               const int count = rib_pause_row(ids, 16);
               rib_pause_focus_row(menu,
                     slot - 4 < count ? slot - 4 : count - 1, false);
            }
         }
         else
         {
            const int column = menu->focus.pause_row() > 2 ? 2 : menu->focus.pause_row();
            menu->focus.pause_row(-1);
            rib_rmlui_focus(menu,
                  rib::Event::select_slot(1 + column), false);
         }
         return 0;
      case RIB_KEY_LEFT:
         if (menu->focus.pause_row() < 0)
         {
            int slot = rib_rmlui_focus_slot(menu->focus.pause_action());
            int row_start = slot <= 3 ? 1 : 4;
            slot = slot == row_start ? row_start + 2 : slot - 1;
            rib_rmlui_focus(menu,
                  rib::Event::select_slot(slot), true);
         }
         else
         {
            char ids[16][64];
            const int count = rib_pause_row(ids, 16);
            rib_pause_focus_row(menu,
                  rib_pause_row_index((const char (*)[64])ids, count) - 1, true);
         }
         return 0;
      case RIB_KEY_RIGHT:
         if (menu->focus.pause_row() < 0)
         {
            int slot = rib_rmlui_focus_slot(menu->focus.pause_action());
            int row_end = slot <= 3 ? 3 : rib::kSlotCount;
            slot = slot == row_end ? row_end - 2 : slot + 1;
            rib_rmlui_focus(menu,
                  rib::Event::select_slot(slot), false);
         }
         else
         {
            char ids[16][64];
            const int count = rib_pause_row(ids, 16);
            rib_pause_focus_row(menu,
                  rib_pause_row_index((const char (*)[64])ids, count) + 1, false);
         }
         return 0;
      case RIB_KEY_OK:
      case RIB_KEY_SELECT:
         if (menu->focus.pause_row() < 0)
            rib_rmlui_perform_action(menu, menu->focus.pause_action());
         else
         {
            /* Press the element itself, so a button from the design opens what
             * its listener opens, without its name in this code. */
            char ids[16][64];
            const int count = rib_pause_row(ids, 16);
            const int index =
                  rib_pause_row_index((const char (*)[64])ids, count);
            if (count > 0)
               rib_rmlui_click_element(ids[index]);
         }
         return 0;
      case RIB_KEY_CANCEL:
      case RIB_KEY_RESUME:
      case RIB_KEY_TOGGLE:
         rib_rmlui_perform_action(menu, rib_rmlui_map_menu_toggle(
               false, false));
         return 0;
      case RIB_KEY_START:
         rib_rmlui_perform_action(menu, RIB_RMLUI_ACTION_SAVE);
         return 0;
      default:
         return 0;
   }
}
