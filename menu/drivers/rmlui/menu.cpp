#include "words.hpp"
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
#include "view.hpp"
#include "../../../audio/volume_range.h"
#include "navigation.hpp"
#include "sounds.hpp"

#ifndef RIB_RMLUI_DEFAULT_ASSETS
#define RIB_RMLUI_DEFAULT_ASSETS "."
#endif

struct Menu
{
   rib::View& view = rib::menu_view();
   bool initialized;
   bool overlay_mode;
   rib::Overlays overlays{view.document};
   rib::Script script{view};
   rib::Shaders shaders{view.lists};
   rib::Discs discs{view.document, view.lists};
   rib::Toggles toggles{view.lists, view.slots};
   rib::Volume volume{view.parts};
   rib::Focus& focus = view.focus;
   rib::Screens& screens = view.screens;
   rib::Controls controls{focus, screens, view.document, view.controls,
         view.lists, view.status, view.hovered};
   bool pointer_pressed;
   rib::Slots& slots = view.slots;
   rib::Navigation navigation{focus, screens, controls, slots,
         view.document, view.lists, view.parts};
};

/* The public runloop callback can come before we allocate the menu. Keep only
 * that pending request here, and the overlay timeline in the menu itself. */
static bool pending_overlay_start;
static Menu *active_menu;

int rib_menu_key(void *data, enum rib_key action);

void rib_rmlui_begin_overlays(void)
{
   if (active_menu)
      active_menu->overlays.begin();
   else
   {
      pending_overlay_start = true;
      rib_host_overlay_frames(true);
   }
}

bool rib_rmlui_overlays_drawing(void)
{
   return pending_overlay_start ||
         (active_menu && active_menu->overlays.drawing());
}

static const char *absolute_data_directory(void)
{
   const char *data = getenv("ROMINABOX_DATA_DIR");

   if (!data || data[0] != '/')
      return NULL;
   return data;
}

/* We load the declarations once. These assignments set the starting state of
 * each feature. There is no document or input code in the loader. */
static void load_design(Menu *menu, const char *assets)
{
   rib_design_declarations *loaded = rib_load_design(assets);
   const rib_design_data *design = rib_design_get(loaded);
   size_t index;
   menu->view.screens.clear_screens();
   menu->discs.configure(*design);
   for (index = 0; index < design->screen_count; ++index)
   {
      const rib_screen_declaration *screen = &design->screens[index];
      menu->view.screens.declare_screen(screen->id, screen->panel, screen->heading,
            screen->footer, screen->button);
   }
   menu->toggles.load(*design, absolute_data_directory());
   menu->overlays.load(*design);
   menu->controls.configure_binds(*design);
   rib_design_free(loaded);
}

void rib_rmlui_notify_state_task(const char *path, int slot,
      bool is_save, bool success)
{
   if (active_menu)
      active_menu->slots.notify_task(path, slot, is_save, success);
}

static void reset_interaction(Menu *menu, bool opening)
{
   if (!menu)
      return;
   if (menu->controls.capture_active)
      menu->controls.cancel_capture(NULL);
   menu->screens.remember("pause");
   menu->navigation.focus_list(0);
   menu->pointer_pressed = false;
   menu->controls.capture_ignore_pointer = false;
   menu->focus.pause_action(RIB_RMLUI_ACTION_RESUME);
   /* When the menu opens, we focus the first button of the row, not a slot. */
   menu->focus.pause_row(0);
   menu->view.clear_intents();
   menu->view.pointer_leave();
   if (opening)
   {
      menu->screens.show_screen("pause");
      menu->slots.focus_action(menu->focus.pause_action());
      menu->view.screens.set_footer_hint(rib::words::ContinueHint);
   }
}

void rib_menu_toggle(void *userdata, bool on)
{
   reset_interaction((Menu*)userdata, on);
}

bool rib_menu_consume_toggle(void *userdata)
{
   Menu *menu = (Menu*)userdata;
   return menu && rib::toggle_stays_in_menu(
         menu->screens.controls_visible() || !string_is_equal(menu->screens.current(), "pause"),
         menu->controls.capture_active);
}

static void perform_action(Menu *menu, const rib::Event& event)
{
   const auto action = event.kind;
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
      rib::play_action_sound(action);
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
         rib::play_action_sound(action);
         menu->discs.sync();
      }
      else if (menu->shaders.apply(id, getenv("ROMINABOX_RML_ASSETS"),
               absolute_data_directory()))
         rib::play_action_sound(action);
      for (row = 0; row < menu->view.lists.visible_row_count(); ++row)
         if (string_is_equal(menu->view.lists.list_row_id(row), id))
         {
            menu->navigation.focus_list(row);
            break;
         }
      return;
   }
   if (action == RIB_RMLUI_ACTION_LIST_PAGE)
   {
      const char *which = event.id.c_str();
      int delta = which && string_is_equal(which, "prev") ? -1 : 1;

      rib::play_action_sound(action);
      if (menu->view.lists.turn_list_page(delta) >= 0)
         menu->navigation.focus_list(0);
      return;
   }
   if (action == RIB_RMLUI_ACTION_PART_TOGGLE)
   {
      rib::play_action_sound(action);
      return;
   }
   if (action == RIB_RMLUI_ACTION_TOGGLE)
   {
      const char *id = event.id.c_str();
      rib::play_action_sound(action);
      menu->toggles.toggle(id, absolute_data_directory());
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
      if (screen_id[0] && menu->view.screens.show_screen(screen_id))
      {
         /* The footer and the heading are in the design, with the screen.
          * Here we keep only the case of the controls screen, where capture
          * and navigation work differently. For any other screen there is
          * nothing to add here. */
         menu->screens.remember(screen_id);
         menu->navigation.focus_list(0);
         if (menu->screens.controls_visible())
         {
            menu->controls.focus(menu->controls.first());
            menu->view.status.set_controls(rib::words::ChooseControl);
            menu->controls.refresh();
         }
         else if (menu->controls.capture_active)
            menu->controls.cancel_capture(rib::words::BindingUnchanged);
         else if (!string_is_equal(screen_id, "pause"))
         {
            char ids[16][64];
            const char *panel = menu->view.screens.screen_panel(menu->screens.current());
            int count = menu->view.document.focusables(panel, ids, 16);
            bool slider = false;
            int index;

            for (index = 0; index < count; ++index)
               if (menu->view.parts.part_is_slider(ids[index]))
                  slider = true;
            /* A slider is the first thing a keyboard should land on: left and
             * right move it. A list with no slider focuses its first row, and
             * then the screen's own controls past that. */
            if (slider)
            {
               menu->focus.position(rib::FocusRegion::Parts, 0);
               menu->view.document.mark_focused(panel, ids[0]);
            }
            else
               menu->navigation.paint_list();
         }
         /* We measure the slider from the box of its track. While the panel
          * is hidden that width is zero, so a paint leaves the thumb where
          * the stylesheet put it, at the quiet end. Paint it again now that
          * the screen is shown. */
         menu->volume.paint();
         menu->shaders.show_running();
      }
      rib::play_action_sound(action);
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
         rib::play_action_sound(action);
         menu->controls.focus(rib::FocusTarget::item(control_index));
         menu->controls.start_capture(control_index);
      }
      return;
   }

   if ((action == RIB_RMLUI_ACTION_SAVE ||
            action == RIB_RMLUI_ACTION_LOAD) &&
         (menu->slots.transfer_pending() || menu->slots.slots_guarded()))
      return;
   if (menu->slots.slots_guarded() && (event.kind == RIB_RMLUI_ACTION_SELECT_SLOT && rib::valid_slot(event.slot)))
      return;

   if (action == RIB_RMLUI_ACTION_LOAD && !menu->slots.load_available())
      return;
   rib::play_action_sound(action);

   switch (action)
   {
      case RIB_RMLUI_ACTION_SAVE:
         menu->slots.request(rib::Slots::Transfer::Save);
         break;
      case RIB_RMLUI_ACTION_LOAD:
         menu->slots.request(rib::Slots::Transfer::Load);
         break;
      case RIB_RMLUI_ACTION_CONTROLS:
      {
         /* Open the screen on the pause row in the design, through its button.
          * We use this path for both the keyboard and the pointer. */
         const char *button = menu->view.screens.pause_screen_button();

         if (button && *button)
         {
            menu->view.document.click_element(button);
            return;
         }
      }
         menu->screens.remember("controls");
         menu->controls.focus(menu->controls.first());
         /* The heading and the footer are in the design, with the
          * screen. */
         menu->view.screens.show_screen("controls");
         menu->view.status.set_controls(rib::words::ChooseControl);
         menu->controls.refresh();
         break;
      case RIB_RMLUI_ACTION_CONTROLS_BACK:
         if (menu->controls.capture_active)
            menu->controls.cancel_capture(rib::words::BindingUnchanged);
         menu->screens.remember("pause");
         menu->focus.pause_action(RIB_RMLUI_ACTION_CONTROLS);
         menu->view.screens.show_screen("pause");
         menu->slots.focus_action(menu->focus.pause_action());
         break;
      case RIB_RMLUI_ACTION_CONTROLS_CANCEL:
         menu->controls.cancel_capture(rib::words::BindingUnchanged);
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
         menu->navigation.focus_pause(event, false);
         break;
      default:
         break;
   }
}

void *rib_menu_create(void)
{
   Menu *menu = new (std::nothrow) Menu{};
   if (!menu)
      return nullptr;
   menu->focus = rib::Focus{};
   menu->slots.reset_transfer();
   menu->slots.set_selected_slot(1);
   menu->focus.pause_action(RIB_RMLUI_ACTION_RESUME);
   menu->focus.pause_row(0);
   menu->screens.remember("pause");
   active_menu = menu;
   if (pending_overlay_start)
   {
      pending_overlay_start = false;
      menu->overlays.begin();
   }
   return menu;
}

void rib_menu_destroy(void *data)
{
   Menu *menu = (Menu*)data;
   if (active_menu == data)
      active_menu = NULL;
   if (menu && menu->controls.capture_active)
      menu->controls.cancel_capture(NULL);
   rib::menu_view().shutdown();
   pending_overlay_start = false;
   delete menu;
   /* We leave the small userdata wrapper of the C adapter to menu_driver_ctl. */
}

void rib_menu_context_destroy(void *data)
{
   Menu *menu = (Menu*)data;
   if (menu && menu->controls.capture_active)
      menu->controls.cancel_capture(NULL);
   rib::menu_view().shutdown();
   if (menu)
      menu->initialized = false;
}

void rib_menu_context_reset(void *data)
{
   Menu *menu = (Menu*)data;
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
   Menu *menu = (Menu*)data;
   rib_pointer pointer;
   const char *asset_directory = getenv("ROMINABOX_RML_ASSETS");
   const char *data_directory = absolute_data_directory();

   if (!menu)
      return;

   if (!menu->initialized)
   {
      if (!asset_directory || !*asset_directory)
         asset_directory = RIB_RMLUI_DEFAULT_ASSETS;
      menu->initialized = menu->view.initialize(asset_directory,
            width, height,
            rib_host_core_gl_context(), menu->controls.catalog);
      if (!menu->initialized)
      {
         RARCH_ERR("[RmlUi] Failed to initialize menu from %s.\n",
               asset_directory);
         menu->overlays.stop();
         rib_host_overlay_frames(false);
         return;
      }
      /* Read the screens and overlays in the design before we show any. */
      load_design(menu, asset_directory);
      menu->shaders.load(asset_directory);
      menu->slots.paint();
      menu->slots.focus_action(menu->focus.pause_action());
      menu->slots.refresh();
      menu->screens.show_screen("pause");
      menu->view.screens.set_footer_hint(rib::words::ContinueHint);
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
         menu->view.set_overlay_mode(menu->overlay_mode);
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
         menu->script.run(menu, {menu->screens.current(), menu->slots.transfer_pending(),
               menu->controls.capture_active, menu->controls.profile_id});
         menu->overlays.update(menu->script.wants_frames());
         menu->view.render(width, height);
         return;
      }
      menu->overlays.update(menu->script.wants_frames());
   }

   pointer = rib_host_pointer();
   {
      bool pointer_pressed =
            pointer.pressed;

      menu->view.pointer_move(pointer.x, pointer.y);
      menu->script.restore_hover();
      menu->view.pointer_button(pointer_pressed);

      if (menu->controls.capture_active && pointer_pressed && !menu->pointer_pressed &&
            (menu->view.hovered.kind == RIB_RMLUI_ACTION_CONTROLS_CANCEL ||
             menu->view.hovered.kind == RIB_RMLUI_ACTION_CONTROLS_BACK))
         menu->controls.capture_ignore_pointer = true;
      if (menu->controls.capture_ignore_pointer && !pointer_pressed)
         menu->controls.capture_ignore_pointer = false;
      menu->pointer_pressed = pointer_pressed;
   }

   {
      const char *drag_id = NULL;
      float drag_fraction = 0.0f;
      if (menu->view.parts.slider_drag(&drag_id, &drag_fraction) && drag_id
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
   menu->script.run(menu, {menu->screens.current(), menu->slots.transfer_pending(),
               menu->controls.capture_active, menu->controls.profile_id});
   menu->script.restore_hover();
   /* After we put the pointer back for the script, so a hovered row is the
    * focused row before the click in this frame. We play no scroll sound,
    * because the pointer did not move by a step. */
   if (!menu->controls.capture_active)
      menu->navigation.focus_list(menu->view.lists.hovered_list_row());

   for (;;)
   {
      auto next_action = menu->view.intents.take();
      if (next_action.kind == RIB_RMLUI_ACTION_NONE)
         break;
      perform_action(menu, next_action);
   }

   menu->controls.poll_capture();

   menu->view.reload_if_changed();
   menu->slots.refresh();
   menu->controls.update_binds(pointer.x, pointer.y,
         !menu->script.wants_frames(),
         !menu->script.wants_frames() || menu->script.has_hover());
   menu->view.render(width, height);

}

int rib_menu_key(void *data, enum rib_key action)
{
   auto *menu = static_cast<Menu*>(data);
   if (menu)
   {
      const auto event = menu->navigation.key(action);
      if (event.kind != RIB_RMLUI_ACTION_NONE)
         perform_action(menu, event);
   }
   return 0;
}
