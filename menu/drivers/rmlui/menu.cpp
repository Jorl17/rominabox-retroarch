#include "words.hpp"
#include <stdlib.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include <new>
#include <stdint.h>
#include <string>
#include <retro_miscellaneous.h>
#include <file/file_path.h>
#include <file/config_file.h>
#include <streams/file_stream.h>
#include "../../../verbosity.h"
#include "../../../rominabox_environment.h"
#include "../../../rominabox_launch.h"
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
#include "game_data.hpp"
#include "hotkeys.hpp"
#include "paging.hpp"
#include "play_hotkeys.hpp"
#include "slots.hpp"
#include "view.hpp"
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
   rib::GameData game_data{view.document, view.status};
   /* The list of each screen that has one. We pass a chosen row to the list of
    * the screen showing, which we find by the declared role of the screen. */
   rib::ListOwner *const owners[3] = {&discs, &shaders, &accounts};
   rib::PlayerSettings settings{view.document, view.parts};
   rib::Focus& focus = view.focus;
   rib::Screens& screens = view.screens;
   rib::CapturePointer capture_pointer;
   rib::PadNames pad_names;
   rib::Controls controls{focus, screens, view.document, view.controls,
         view.lists, view.status, view.hovered, capture_pointer, pad_names};
   rib::Hotkeys hotkeys{view.document, focus, screens, view.status,
         view.intents, view.hovered, capture_pointer, controls, pad_names};
   rib::PlayHotkeys play_hotkeys{hotkeys.read(), view.slots, overlays};
   bool pointer_pressed;
   /* Whether a capture on CONTROLS or HOTKEYS was in progress at the end of
    * the previous frame. */
   bool was_capturing = false;
   rib::Slots& slots = view.slots;
   rib::Navigation navigation{focus, screens, controls,
         view.document, view.lists, view.parts};
};

/* The public runloop callback can come before we allocate the menu. Keep only
 * that pending request here, and the overlay timeline in the menu itself. */
static bool pending_overlay_start;
static Menu *active_menu;

int rib_menu_key(void *data, enum rib_key action);
static void perform_action(Menu *menu, const rib::Event& event);

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

/* The time at which we made the game wait for the splash. */
static int64_t game_held_since;

bool rib_rmlui_game_held(void)
{
   /* Whatever fails in the menu, we do not make the game wait any longer. */
   static const int64_t longest_us = 3000000;
   const bool held = pending_overlay_start
         || (active_menu && active_menu->overlays.holding_game());
   if (!held)
      return false;
   if (!game_held_since)
      game_held_since = rib_host_time_us();
   return rib_host_time_us() - game_held_since < longest_us;
}

bool rib_rmlui_overlays_drawing(void)
{
   return pending_overlay_start ||
         (active_menu && (active_menu->overlays.drawing() || rib_achievements_has_unlocks()));
}

bool rib_rmlui_text_event(bool down, unsigned key, uint32_t character, uint16_t modifiers)
{
   return rib_rmlui_reads_input() &&
         active_menu->achievements.physical(down, key, character, modifiers);
}

bool rib_rmlui_typing(void)
{
   return rib_rmlui_reads_input() && active_menu->achievements.typing();
}

bool rib_rmlui_reads_input(void)
{
   return active_menu && active_menu->initialized && rib_host_menu_open();
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
/* Whether we are capturing a binding, on either screen with a capture. */
static bool capturing(const Menu *menu)
{
   return menu && (menu->controls.capture_active || menu->hotkeys.capturing());
}

/* The state of the menu in this frame, for the test script. */
static rib::ScriptObservation observe(Menu *menu)
{
   return {menu->screens.current().c_str(), menu->slots.transfer_pending(),
         capturing(menu), menu->controls.profile_id.c_str(),
         menu->controls.binds_list(), menu->overlays.settled()};
}

static void cancel_captures(Menu *menu)
{
   if (menu->controls.capture_active)
      menu->controls.cancel_capture();
   menu->hotkeys.cancel_capture();
}

unsigned rib_rmlui_menu_keys(unsigned *codes, unsigned capacity)
{
   if (!active_menu || !codes || rib_rmlui_typing())
      return 0;
   unsigned count = 0;
   for (unsigned code : active_menu->hotkeys.read().keys(rib::Hotkey::Menu))
      if (count < capacity)
         codes[count++] = code;
   return count;
}

bool rib_rmlui_menu_pad_held(void)
{
   return active_menu && active_menu->hotkeys.read().held(rib::Hotkey::Menu, false);
}

void rib_rmlui_menu_buttons(uint32_t *buttons, unsigned ok, unsigned cancel)
{
   if (!active_menu || !buttons || ok >= 32 || cancel >= 32)
      return;
   const rib::HotkeyBindings& bindings = active_menu->hotkeys.read();
   const bool keys = !rib_rmlui_typing();
   uint32_t read_here = (1u << ok) | (1u << cancel);
   for (unsigned bind : bindings.menu_pad_binds())
      if (bind < 16)
         read_here |= 1u << bind;
   *buttons &= ~read_here;
   if (bindings.held(rib::Hotkey::Confirm, keys))
      *buttons |= 1u << ok;
   if (bindings.held(rib::Hotkey::Back, keys))
      *buttons |= 1u << cancel;
}

bool rib_rmlui_menu_hotkey_key(unsigned code)
{
   return active_menu && active_menu->hotkeys.read().menu_key(code);
}

void rib_rmlui_play_hotkeys(void)
{
   using Doing = rib::PlayHotkeys::Doing;
   if (!active_menu)
      return;
   active_menu->play_hotkeys.frame(capturing(active_menu) ? Doing::Capturing
         : rib_rmlui_typing() ? Doing::Typing
         : rib_host_menu_open() || active_menu->overlays.holding_game() ? Doing::Waiting
         : Doing::Playing);
}

void rib_rmlui_fast_forward(bool *hold, bool *toggle)
{
   *hold = *toggle = false;
   if (!active_menu || !active_menu->play_hotkeys.holding(rib::Hotkey::FastForward))
      return;
   if (active_menu->hotkeys.read().mode(rib::Hotkey::FastForward) == 0)
      *hold = true;
   else
      *toggle = true;
}

static rib::ListOwner *showing_list(Menu *menu)
{
   const rib::ScreenRole role = menu->screens.current_role();
   for (rib::ListOwner *owner : menu->owners)
      if (role != rib::ScreenRole::None && owner->role() == role)
         return owner;
   return nullptr;
}

/* Prepare a screen after we show it, apart from its focus. We call this for
 * every screen, through Navigation. */
static void screen_shown(Menu *menu)
{
   const rib::ScreenRole role = menu->screens.current_role();
   menu->controls.screen_shown(role == rib::ScreenRole::Controls);
   menu->hotkeys.screen_shown(role == rib::ScreenRole::Hotkeys);
   menu->achievements.screen_shown(role == rib::ScreenRole::Achievements);
   menu->game_data.screen_shown(role == rib::ScreenRole::Data);
   /* We measure the slider from the box of the track. While the panel is
    * hidden its width is zero, so painting leaves the thumb at its position in
    * the stylesheet, at the low end. Paint again now that we show the screen. */
   menu->settings.paint();
   if (rib::ListOwner *owner = showing_list(menu))
      owner->shown();
}

/* Give each feature what the design declares for it. We declare screens on
 * the loaded document, so call this after the document loads. */
static void apply_design(Menu *menu, const rib::DesignDeclarations& design, const char *data)
{
   menu->screens.clear_screens();
   for (const rib::ScreenDeclaration& screen : design.screens)
      menu->screens.declare_screen(screen);
   menu->discs.configure(design);
   menu->accounts.configure(design);
   menu->settings.load(design, data);
   menu->overlays.load(design);
   menu->controls.configure_binds(design.binds);
}

bool rib_rmlui_notify_state_task(const char *path, int slot,
      bool is_save, bool success)
{
   return active_menu && active_menu->slots.notify_task(path, slot, is_save, success);
}

/* The open dialog, if any, that the player cannot leave with the arrows. */
static Rml::Element *open_dialog(Menu *menu)
{
   if (!menu->view.document.root())
      return nullptr;
   if (menu->game_data.modal())
      if (auto *dialog = menu->view.document.root()->GetElementById(rib::document_contract::DataImportDialog))
         if (!rib::hidden(dialog))
            return dialog;
   if (!menu->achievements.modal())
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
   menu->focus.press(!capturing(menu)
         && menu->hotkeys.read().held(rib::Hotkey::Confirm, !rib_rmlui_typing()));
}

/* Read what we show of the running game when the menu opens. That is the
 * aspect ratio of the game, which can change while it runs, and the slots,
 * which a save by hotkey can change. */
static void read_game(Menu *menu)
{
   menu->view.document.show_game_shape(rib_host_game_aspect());
   menu->slots.refresh();
}

static void reset_interaction(Menu *menu, bool opening)
{
   /* After a new video driver there is no document until we build it again
    * in the next frame, as for a menu opened afresh. Opening or closing the
    * menu before then has nothing to reset. */
   if (!menu || !menu->initialized)
      return;
   menu->achievements.leave_form();
   cancel_captures(menu);
   menu->pointer_pressed = false;
   menu->capture_pointer.frame(false, false);
   /* When the menu closes during a drag, we end the drag where it is and keep
    * it, as if released. We drop everything else in the queue. */
   const rib::Event cut_short = menu->view.pointer_leave();
   menu->view.clear_intents();
   if (cut_short.kind != RIB_RMLUI_ACTION_NONE)
      perform_action(menu, cut_short);
   if (!opening)
   {
      menu->navigation.close();
      return;
   }
   read_game(menu);
   /* While the menu is open, the document is the whole menu and not only what
    * we last drew over the game, so we enter its screen where it is visible. */
   menu->overlay_mode = false;
   menu->view.set_overlay_mode(false);
   menu->navigation.open();
   settle_focus(menu);
}

void rib_menu_toggle(void *userdata, bool on)
{
   /* When the menu opens or closes, we have run or will run frames of the
    * game, so the frame we take a picture of is the game as it is now. */
   if (userdata)
      ((Menu*)userdata)->slots.forget_load();
   reset_interaction((Menu*)userdata, on);
   rib_host_show_pointer(on);
}

bool rib_menu_consume_toggle(void *userdata)
{
   Menu *menu = (Menu*)userdata;
   return menu && (menu->achievements.modal() || menu->game_data.modal() || capturing(menu)
         || !menu->screens.showing(rib::ScreenRole::Pause));
}

/* Pass an event to its feature, where we decide what to do with it, if
 * anything. For example, we give every press to a capture and to an open
 * dialog, and run one slot transfer at a time. Here we only move between
 * screens and leave the menu. */
static void perform_action(Menu *menu, const rib::Event& event)
{
   if (!menu || menu->game_data.handle(event) || menu->achievements.handle(event) || menu->hotkeys.handle(event)
         || menu->controls.handle(event)
         || menu->slots.handle(event) || menu->settings.handle(event))
      return;

   switch (event.kind)
   {
      case RIB_RMLUI_ACTION_LIST_CHOOSE:
      case RIB_RMLUI_ACTION_LIST_ACTION:
      {
         rib::ListOwner *owner = showing_list(menu);
         const char *id = event.id.c_str();
         if (owner && (event.kind == RIB_RMLUI_ACTION_LIST_CHOOSE ? owner->choose(id) : owner->act(id)))
            rib::play_action_sound(event.kind);
         menu->focus.set(id);
         /* When we have finished with the list of a screen, as on QUICK SIGN IN
          * once sign-in has started, we show the screen that the list
          * returns. */
         const std::string& next = owner ? menu->screens.with_role(owner->leave_for()) : std::string();
         if (!next.empty())
            menu->navigation.show(next);
         break;
      }
      case RIB_RMLUI_ACTION_LIST_PAGE:
         rib::play_action_sound(event.kind);
         menu->navigation.turn_page(event.direction, menu->focus.current());
         break;
      case RIB_RMLUI_ACTION_SHOW_SCREEN:
         menu->navigation.show(menu->discs.redirect(event.id));
         break;
      case RIB_RMLUI_ACTION_CONTROLS_BACK:
         /* Go back to the screen from which the player opened this one. */
         menu->navigation.back();
         break;
      case RIB_RMLUI_ACTION_RESUME:
         rib::play_action_sound(event.kind);
         rib_host_resume();
         break;
      case RIB_RMLUI_ACTION_QUIT:
         rib::play_action_sound(event.kind);
         rib_host_quit();
         break;
      case RIB_RMLUI_ACTION_FORGET:
         rib::play_action_sound(event.kind);
         rib_host_forget();
         break;
      case RIB_RMLUI_ACTION_RESTART:
         rib::play_action_sound(event.kind);
         rib_host_restart();
         rib_host_resume();
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
   menu->navigation.on_shown([menu] { screen_shown(menu); });
   menu->navigation.on_capturing([menu] { return capturing(menu); });
   {
      /* We read the input that opens the menu, the slot for hotkey saves and
       * the shaders, which we use while the game runs, before we have ever
       * drawn the menu. */
      const rib_environment_value assets = rib_owned(rib_environment(RIB_ENV_RML_ASSETS));
      const rib_environment_value data = rib_owned(rib_data_directory());
      menu->pad_names.load(assets && *assets ? assets.get() : RIB_RMLUI_DEFAULT_ASSETS);
      menu->hotkeys.load(assets && *assets ? assets.get() : RIB_RMLUI_DEFAULT_ASSETS,
            data.get());
      menu->slots.reset_transfer();
      menu->slots.load(data.get());
      menu->shaders.load(assets && *assets ? assets.get() : RIB_RMLUI_DEFAULT_ASSETS,
            data.get());
   }
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
   if (menu)
      cancel_captures(menu);
   if (menu) menu->achievements.context_lost();
   rib::menu_view().shutdown();
   pending_overlay_start = false;
   delete menu;
   /* We leave the small userdata wrapper of the C adapter to menu_driver_ctl. */
}

void rib_menu_context_destroy(void *data)
{
   Menu *menu = (Menu*)data;
   if (menu)
      cancel_captures(menu);
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

/* Load the document, the declarations of the design and everything from the
 * game that we show in the menu, once for each document. */
static bool initialize(Menu *menu, const char *assets, int width, int height)
{
   const rib_environment_value data_directory = rib_owned(rib_data_directory());
   const char *data = data_directory.get();
   const rib::DesignDeclarations design = rib::load_design(assets);
   /* Read the words of the design before we write any. */
   rib::use_words(design.words);
   if (!menu->view.initialize(assets, design.fonts, width, height,
            rib_host_core_gl_context(), menu->controls.catalog))
   {
      RARCH_ERR("[RmlUi] Failed to initialize menu from %s: its %s, or the "
            "fonts its %s lists, would not load.\n", assets, rib::files::Menu,
            rib::files::Design);
      menu->overlays.stop();
      rib_host_overlay_frames(false);
      return false;
   }
   /* Declare the screens and overlays of the design before we show any. */
   apply_design(menu, design, data);
   menu->achievements.bind();
   menu->accounts.bind();
   menu->hotkeys.bind();
   menu->shaders.load(assets, data);
   menu->slots.paint();
   read_game(menu);
   menu->navigation.open();
   if (!menu->controls.loaded)
   {
      menu->controls.load(assets, data);
      menu->controls.loaded = true;
   }
   menu->controls.refresh();
   menu->settings.attach();
   /* We split the lists into pages when the document loaded, before we showed
    * or hid the rows of each feature. We split them again now, so a list on a
    * screen we have not shown yet has the right number of pages. */
   rib::paging::resplit(menu->view.document.root());
   rib::load_level_cue(assets);
   RARCH_LOG("[RmlUi] Loaded menu from %s.\n", assets);
   return true;
}

void rib_menu_frame(void *data, int width, int height)
{
   Menu *menu = (Menu*)data;

   if (!menu)
      return;

   if (!menu->initialized)
   {
      const rib_environment_value assets = rib_owned(rib_environment(RIB_ENV_RML_ASSETS));
      menu->initialized = initialize(menu,
            assets && *assets ? assets.get() : RIB_RMLUI_DEFAULT_ASSETS, width, height);
      if (!menu->initialized)
         return;
      /* We treat a new document as the whole menu until we change that below,
       * also when we build it for a new video driver while the game runs. */
      menu->overlay_mode = false;
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
         menu->script.run(menu, observe(menu));
         menu->overlays.update(menu->script.wants_frames());
         menu->view.render(width, height);
         return;
      }
      menu->overlays.update(menu->script.wants_frames());
   }

   menu->view.render(width, height);
}

/* Run queued actions, from the player or the script, between frames in the
 * RetroArch loop, never while we draw the menu in the video driver. Applying a
 * shader, for example, gives the GL context back to a hardware core, and the
 * rest of a frame drawn after that never reaches the window. */
void rib_menu_update(void *data)
{
   Menu *menu = (Menu*)data;
   rib_pointer pointer;

   if (!menu || !menu->initialized || !rib_host_menu_open())
      return;

   rib_host_rumble_frame();
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
         const std::string kept = capturing(menu)
               ? menu->focus.current_id() : std::string();
         menu->view.pointer_button(pointer_pressed);
         if (!kept.empty())
            menu->focus.set(kept.c_str());
      }

      menu->capture_pointer.frame(pointer_pressed,
            capturing(menu) && pointer_pressed && !menu->pointer_pressed
            && rib::CapturePointer::leaves(menu->view.hovered.kind));
      menu->pointer_pressed = pointer_pressed;
   }

   {
      const char *drag_id = NULL;
      float drag_fraction = 0.0f;
      if (!menu->view.parts.slider_drag(&drag_id, &drag_fraction)
            || !menu->settings.slide(drag_id, drag_fraction, false))
         /* We measure a slider in pixels, which depend on the window and on the
          * layout of the track, so we measure it each frame and write only
          * what changed. */
         menu->settings.paint();
   }

   /* Before we empty the queue, so we handle a scripted click in this frame,
    * in the same loop as a click from the player. We put back a hover from
    * the script after the click, because the pointer move above followed the
    * mouse and would have removed the hover. The disc entry starts hidden.
    * Fill it before the click from the script, or the click goes to a button
    * that is still display:none in the document. */
   menu->discs.sync();
   menu->script.run(menu, observe(menu));
   menu->script.restore_hover();
   /* Once we have put the pointer back after the script, we silently focus
    * the stop the pointer moved onto, before the click of this frame. We never
    * take the focus away from the keys for a pointer at rest. */
   if (!capturing(menu))
      menu->view.follow_pointer();

   for (;;)
   {
      auto next_action = menu->view.intents.take();
      if (next_action.kind == RIB_RMLUI_ACTION_NONE)
         break;
      perform_action(menu, next_action);
   }

   menu->controls.poll_capture();
   menu->hotkeys.poll_capture();
   /* When a capture has ended since the previous frame, for any reason, the
    * player can still be pressing an input bound to a hotkey. We ignore those
    * inputs until they are released. */
   if (menu->was_capturing && !capturing(menu))
      menu->hotkeys.ignore_pressed_until_released();
   menu->was_capturing = capturing(menu);
   menu->slots.follow();
   menu->controls.update_binds(pointer.x, pointer.y,
         !menu->script.wants_frames());
   settle_focus(menu);
}

int rib_menu_key(void *data, enum rib_key action)
{
   auto *menu = static_cast<Menu*>(data);
   if (menu)
   {
      if (!menu->achievements.key(action) && !menu->game_data.key(action))
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
