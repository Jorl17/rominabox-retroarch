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
#include "achievements.hpp"
#include "script.hpp"
#include "shaders.hpp"
#include "discs.hpp"
#include "saved_accounts.hpp"
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
   rib::Achievements achievements{view.document, view.lists, view.intents, overlays};
   rib::Script script{view};
   rib::Shaders shaders{view.lists};
   rib::Discs discs{view.document, view.lists};
   rib::SavedAccounts accounts{view.document, view.lists, view.intents};
   /* The list of each screen that has one. We pass a chosen row to the list of
    * the screen showing, which we find by the declared role of the screen. */
   rib::ListOwner *const owners[3] = {&discs, &shaders, &accounts};
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
         (active_menu && (active_menu->overlays.drawing() || rib_achievements_has_unlocks()));
}

bool rib_rmlui_text_event(bool down, unsigned key, uint32_t character, uint16_t modifiers)
{
   return active_menu && active_menu->initialized && rib_host_menu_open() &&
         active_menu->achievements.physical(down, key, character, modifiers);
}

bool rib_rmlui_begin_native_text(void)
{
   return active_menu && active_menu->initialized && rib_host_menu_open() &&
         active_menu->achievements.begin_native_input();
}

bool rib_rmlui_allow_quit(void)
{
   if (!active_menu || active_menu->achievements.allow_quit()) return true;
   rib_host_open_menu();
   return false;
}

/* The list of the screen showing now, if it has one. */
static rib::ListOwner *showing_list(Menu *menu)
{
   const char *role = menu->screens.role_of(menu->screens.current());
   for (rib::ListOwner *owner : menu->owners)
      if (role[0] && string_is_equal(owner->role(), role))
         return owner;
   return nullptr;
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
   menu->accounts.configure(*design);
   for (index = 0; index < design->screen_count; ++index)
   {
      const rib_screen_declaration *screen = &design->screens[index];
      menu->view.screens.declare_screen(screen->id, screen->panel, screen->heading,
            screen->footer, screen->button, screen->role);
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

/* The open dialog, if any, that the player cannot leave with the arrows. */
static Rml::Element *open_dialog(Menu *menu)
{
   if (!menu->achievements.modal() || !menu->view.document.root())
      return nullptr;
   for (const char *id : {rib::document_contract::AchievementsConfirmation,
         rib::document_contract::AchievementsStartup})
      if (auto *dialog = menu->view.document.root()->GetElementById(id))
         if (!rib::hidden(dialog))
            return dialog;
   return nullptr;
}

/* Call after anything that can move the focus. Keep it in an open dialog,
 * and mark it. */
static void settle_focus(Menu *menu)
{
   menu->navigation.hold(open_dialog(menu));
   /* When the focus ends where nothing can be used, as when closing a form
    * moves it from a field to the form, we move it to the screen start. */
   if (Rml::Element *focused = menu->focus.current())
      if (!menu->focus.stop(focused))
         menu->navigation.enter();
   menu->focus.paint();
}

static void reset_interaction(Menu *menu, bool opening)
{
   if (!menu)
      return;
   menu->achievements.leave_form();
   if (menu->controls.capture_active)
      menu->controls.cancel_capture(NULL);
   menu->screens.remember("pause");
   menu->pointer_pressed = false;
   menu->controls.capture_ignore_pointer = false;
   menu->view.clear_intents();
   menu->view.pointer_leave();
   if (opening)
   {
      /* Pause, with CONTINUE highlighted. */
      menu->navigation.open();
      menu->view.screens.set_footer_hint(rib::words::ContinueHint);
      settle_focus(menu);
   }
}

void rib_menu_toggle(void *userdata, bool on)
{
   reset_interaction((Menu*)userdata, on);
}

bool rib_menu_consume_toggle(void *userdata)
{
   Menu *menu = (Menu*)userdata;
   return menu && (menu->achievements.modal() || rib::toggle_stays_in_menu(
         menu->screens.controls_visible() || !string_is_equal(menu->screens.current(), "pause"),
         menu->controls.capture_active));
}

/* Prepare a screen after we show it, apart from its focus. */
static void screen_shown(Menu *menu)
{
   if (menu->screens.controls_visible())
   {
      menu->view.status.set_controls(rib::words::ChooseControl);
      menu->controls.refresh();
   }
   else if (menu->controls.capture_active)
      menu->controls.cancel_capture(rib::words::BindingUnchanged);
   /* We measure the slider from the box of the track. While the panel is
    * hidden its width is zero, so painting leaves the thumb at its position in
    * the stylesheet, at the low end. Paint again now that we show the screen. */
   menu->volume.paint();
   menu->shaders.show_running();
   if (rib::ListOwner *owner = showing_list(menu))
      owner->shown();
   if (string_is_equal(menu->screens.role_of(menu->screens.current()), "achievements"))
      menu->achievements.shown();
}

/* When we have finished with the list of a screen, as on QUICK SIGN IN once
 * sign-in has started, we show the screen that the list returns. */
static void follow_list(Menu *menu, rib::ListOwner *owner)
{
   const char *role = owner ? owner->leave_for() : nullptr;
   const char *screen = role ? menu->screens.with_role(role) : "";
   if (screen[0] && menu->navigation.show(screen))
      screen_shown(menu);
}

static void perform_action(Menu *menu, const rib::Event& event)
{
   const auto action = event.kind;
   int control_index;

   if (!menu)
      return;

   if (menu->achievements.handle(event)) return;

   if (menu->controls.capture_active &&
       action != RIB_RMLUI_ACTION_CONTROLS_CANCEL &&
       action != RIB_RMLUI_ACTION_CONTROLS_BACK)
      return;

   if (action == RIB_RMLUI_ACTION_DEVICE_PICKER_TOGGLE)
   {
      menu->controls.toggle_picker();
      menu->navigation.picker();
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
   if (action == RIB_RMLUI_ACTION_LIST_CHOOSE || action == RIB_RMLUI_ACTION_LIST_ACTION)
   {
      const char *id = event.id.c_str();
      rib::ListOwner *owner = showing_list(menu);
      const bool done = owner && (action == RIB_RMLUI_ACTION_LIST_CHOOSE ?
            owner->choose(id) : owner->act(id));

      if (done)
         rib::play_action_sound(action);
      menu->focus.set(id);
      follow_list(menu, owner);
      return;
   }
   if (action == RIB_RMLUI_ACTION_LIST_PAGE)
   {
      const char *which = event.id.c_str();
      int delta = which && string_is_equal(which, "prev") ? -1 : 1;

      rib::play_action_sound(action);
      menu->navigation.turn_page(delta, menu->focus.current());
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
      if (!string_is_equal(screen_id, "achievements")) menu->achievements.leave_form();
      /* The heading and footer of the screen come from the design, and we
       * set the focus and the sound in Navigation. Here we keep only the
       * fact used in Menu, which is that on the controls screen we capture
       * differently. */
      if (screen_id[0] && menu->navigation.show(screen_id))
         screen_shown(menu);
      return;
   }

   if (action == RIB_RMLUI_ACTION_DEVICE_PICKER_CHOOSE)
   {
      menu->controls.choose_device(event.id.c_str());
      menu->navigation.picker();
      return;
   }

   if (action == RIB_RMLUI_ACTION_CONTROLS_BACK)
   {
      /* Go back to the screen from which the player opened this one. */
      if (menu->controls.capture_active)
         menu->controls.cancel_capture(rib::words::BindingUnchanged);
      menu->navigation.back_to_opener();
      screen_shown(menu);
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
         /* The heading and the footer are in the design, with the
          * screen. */
         if (menu->navigation.show("controls"))
            screen_shown(menu);
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
         menu->navigation.select_slot(event.slot);
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
   menu->slots.reset_transfer();
   menu->slots.set_selected_slot(1);
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
   if (menu) menu->achievements.context_lost();
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
   if (menu) menu->achievements.context_lost();
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
      menu->achievements.bind();
      menu->accounts.bind();
      menu->shaders.load(asset_directory, absolute_data_directory());
      menu->slots.paint();
      menu->slots.refresh();
      menu->navigation.open();
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
      menu->controls.refresh();
      menu->volume.initialize();
      /* After the slots, so the lock from a switch replaces the slot count. */
      menu->toggles.apply();
      RARCH_LOG("[RmlUi] Loaded menu from %s.\n", asset_directory);
   }

   menu->achievements.update();
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
      {
         /* While we capture a binding, a press moves nothing. In RmlUi a press
          * focuses what is under it, so we give the focus back to the
          * control being bound. The press can still reach CANCEL. */
         const std::string kept = menu->controls.capture_active
               ? menu->focus.current_id() : std::string();
         menu->view.pointer_button(pointer_pressed);
         if (!kept.empty())
            menu->focus.set(kept.c_str());
      }

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
   /* Once we have put the pointer back after the script, we silently focus
    * the stop the pointer moved onto, before the click of this frame. We never
    * take the focus away from the keys for a pointer at rest. */
   if (!menu->controls.capture_active)
      menu->view.follow_pointer();

   for (;;)
   {
      auto next_action = menu->view.intents.take();
      if (next_action.kind == RIB_RMLUI_ACTION_NONE)
         break;
      perform_action(menu, next_action);
   }

   menu->controls.poll_capture();

   if (menu->view.reload_if_changed())
   {
      menu->achievements.context_lost();
      load_design(menu, asset_directory);
      menu->achievements.bind();
      menu->accounts.bind();
      menu->screens.show_screen(menu->screens.current());
      menu->navigation.enter();
   }
   menu->slots.refresh();
   menu->controls.update_binds(pointer.x, pointer.y,
         !menu->script.wants_frames());
   settle_focus(menu);
   menu->view.render(width, height);

}

int rib_menu_key(void *data, enum rib_key action)
{
   auto *menu = static_cast<Menu*>(data);
   if (menu)
   {
      if (!menu->achievements.key(action))
      {
         const auto event = menu->navigation.key(action);
         if (event.kind != RIB_RMLUI_ACTION_NONE)
            perform_action(menu, event);
      }
      /* On OK we click the focused element and queue its action in its
       * listener. Run it now, as the action of the key, not a frame later. */
      for (auto next = menu->view.intents.take(); next.kind != RIB_RMLUI_ACTION_NONE;
            next = menu->view.intents.take())
         perform_action(menu, next);
      settle_focus(menu);
   }
   return 0;
}
