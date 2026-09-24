#include "navigation.hpp"
#include "focus.hpp"
#include "screens.hpp"
#include "controls.hpp"
#include "slots.hpp"
#include "sounds.hpp"
#include "document.hpp"
#include "lists.hpp"
#include "parts.hpp"
#include <string/stdstring.h>

namespace rib {
int Navigation::pause_row(char ids[][64], int capacity)
{
   char all[16][64];
   int found = document.focusables("pause-panel", all, 16);
   int count = 0;
   int index;

   for (index = 0; index < found && count < capacity; ++index)
   {
      if (document.element_disabled(all[index]))
         continue;
      strlcpy(ids[count], all[index], 64);
      ++count;
   }
   return count;
}

int Navigation::pause_row_index(const char ids[][64], int count)
{
   const char *focused = focus.pause_element().c_str();
   int index;

   if (focused && *focused)
      for (index = 0; index < count; ++index)
         if (string_is_equal(ids[index], focused))
            return index;
   return 0;
}

void Navigation::focus_pause_row(int index,
      bool direction_up)
{
   char ids[16][64];
   const int count = pause_row(ids, 16);

   if (count <= 0)
      return;
   index = Focus::ring(index, count, 0);
   focus.pause_row(index);
   slots.focus_element(ids[index]);
   play_move_sound(direction_up);
}

void Navigation::focus_pause(rib::Event focused,
      bool direction_up)
{
   bool changed;

   if (focused.kind == RIB_RMLUI_ACTION_LOAD &&
       !slots.load_available())
      focused = direction_up ? RIB_RMLUI_ACTION_SAVE :
            RIB_RMLUI_ACTION_CONTROLS;
   changed = !focus.pause_action().same_target(focused);
   focus.pause_action(focused);
   /* Focusing an action or a slot here moves the focus off the row. We track
    * the row by id, not by this enum. */
   focus.pause_row(-1);
   slots.focus_action(focused);
   if (focused.kind == RIB_RMLUI_ACTION_SELECT_SLOT && valid_slot(focused.slot))
      slots.set_selected_slot(focused.slot);
   if (changed)
      play_move_sound(direction_up);
}

void Navigation::paint_list()
{
   const int rows = lists.visible_row_count();

   if (focus.position(rib::FocusRegion::List) < rows)
   {
      lists.focus_list_row(focus.position(rib::FocusRegion::List));
      lists.focus_list_control(-1);
      return;
   }
   lists.focus_list_row(-1);
   lists.focus_list_control(focus.position(rib::FocusRegion::List) - rows);
}

void Navigation::focus_list(int index)
{
   if (index < 0)
      return;
   focus.position(rib::FocusRegion::List, index);
   paint_list();
}

Event Navigation::part_key(rib_key action)
{
   char ids[16][64];
   const char *panel = screens.screen_panel(screens.current());
   int count = document.focusables(panel, ids, 16);

   if (count <= 0)
      return {};
   if (focus.position(rib::FocusRegion::Parts) < 0 || focus.position(rib::FocusRegion::Parts) >= count)
      focus.position(rib::FocusRegion::Parts, 0);

   switch (action)
   {
      case RIB_KEY_UP:
      case RIB_KEY_DOWN:
         focus.move(FocusRegion::Parts, count, action == RIB_KEY_UP ? -1 : 1);
         document.mark_focused(panel, ids[focus.position(FocusRegion::Parts)]);
         play_move_sound(action == RIB_KEY_UP);
         return {};
      case RIB_KEY_LEFT:
      case RIB_KEY_RIGHT:
         if (parts.part_is_slider(ids[focus.position(rib::FocusRegion::Parts)]))
            parts.nudge_slider(ids[focus.position(rib::FocusRegion::Parts)],
                  action == RIB_KEY_RIGHT ? 1 : -1);
         return {};
      case RIB_KEY_OK:
      case RIB_KEY_SELECT:
         document.click_element(ids[focus.position(rib::FocusRegion::Parts)]);
         return {};
      case RIB_KEY_CANCEL:
      case RIB_KEY_RESUME:
      case RIB_KEY_TOGGLE:
         return back();
      default:
         return {};
   }
}

Event Navigation::back()
{
   // Press the Back button of the screen, with its declared destination.
   if (!lists.click_screen_back())
      return RIB_RMLUI_ACTION_CONTROLS_BACK;
   return {};
}

Event Navigation::key(rib_key action)
{
   if (screens.controls_visible()) return controls_key(action);
   if (string_is_equal(screens.current(), "pause")) return pause_key(action);

   char ids[16][64];
   const char *panel = screens.screen_panel(screens.current());
   const int count = document.focusables(panel, ids, 16);
   const int rows = lists.visible_row_count();
   // In a panel with both, Left and Right move a slider, not the list page.
   bool slider = false;
   for (int index = 0; index < count; ++index)
      if (parts.part_is_slider(ids[index])) slider = true;
   return slider || rows <= 0 ? part_key(action) : list_key(action, rows);
}

Event Navigation::list_key(rib_key action, int rows)
{
   const int stops = rows + lists.list_control_count();

   switch (action)
   {
      case RIB_KEY_UP:
      case RIB_KEY_DOWN:
         if (stops > 0)
         {
            focus_list(Focus::ring(focus.position(FocusRegion::List), stops,
                  action == RIB_KEY_UP ? -1 : 1));
            play_move_sound(action == RIB_KEY_UP);
         }
         return {};
      case RIB_KEY_LEFT:
      case RIB_KEY_RIGHT:
         if (lists.turn_list_page(action == RIB_KEY_LEFT ? -1 : 1) >= 0)
         {
            focus_list(0);
            play_action_sound(RIB_RMLUI_ACTION_LIST_PAGE);
         }
         return {};
      case RIB_KEY_OK:
      case RIB_KEY_SELECT:
         if (focus.position(rib::FocusRegion::List) >= rows)
         {
            /* We go through the listener of the element, as for a pointer, where
             * we already handle the switch and BACK. */
            document.click_element(
                  lists.list_control_id(focus.position(rib::FocusRegion::List) - rows));
         }
         else if (rows > 0)
         {
            return {RIB_RMLUI_ACTION_LIST_CHOOSE,
                  lists.list_row_id(focus.position(rib::FocusRegion::List))};
         }
         return {};
      case RIB_KEY_CANCEL:
      case RIB_KEY_RESUME:
      case RIB_KEY_TOGGLE:
         return back();
      default:
         return {};
   }
}

Event Navigation::controls_key(rib_key action)
{
   if (controls.capture_active)
   {
      if (action == RIB_KEY_CANCEL || action == RIB_KEY_RESUME ||
          action == RIB_KEY_TOGGLE)
         return RIB_RMLUI_ACTION_CONTROLS_CANCEL;
      return {};
   }

   switch (action)
   {
      case RIB_KEY_UP:
      case RIB_KEY_LEFT:
      case RIB_KEY_DOWN:
      case RIB_KEY_RIGHT:
      {
         const bool up = action == RIB_KEY_UP || action == RIB_KEY_LEFT;
         play_move_sound(up);
         controls.focus(controls.step(up ? -1 : 1));
         return {};
      }
      case RIB_KEY_OK:
      case RIB_KEY_SELECT:
         if (focus.target(rib::FocusRegion::Controls).kind == rib::FocusTarget::Kind::Item)
            controls.start_capture(focus.position(rib::FocusRegion::Controls));
         else if (focus.target(rib::FocusRegion::Controls).kind == rib::FocusTarget::Kind::Reset)
            return RIB_RMLUI_ACTION_CONTROLS_RESET;
         else
            return RIB_RMLUI_ACTION_CONTROLS_BACK;
         return {};
      case RIB_KEY_START:
         return RIB_RMLUI_ACTION_CONTROLS_RESET;
      case RIB_KEY_CANCEL:
      case RIB_KEY_RESUME:
      case RIB_KEY_TOGGLE:
         return back();
      default:
         return {};
   }
}

Event Navigation::pause_key(rib_key action)
{
   switch (action)
   {
      case RIB_KEY_UP:
         if (focus.pause_row() < 0)
         {
            int slot = focus.pause_action().slot;
            if (slot > 3)
               focus_pause(rib::Event::select_slot(slot - 3), true);
            else
            {
               /* From the top row of slots to the buttons, in the column of
                * the slot. When the row has fewer than three buttons, we
                * focus its last button. */
               char ids[16][64];
               const int count = pause_row(ids, 16);
               focus_pause_row(
                     slot - 1 < count ? slot - 1 : count - 1, true);
            }
         }
         else
         {
            const int column = focus.pause_row() > 2 ? 2 : focus.pause_row();
            focus.pause_row(-1);
            focus_pause(
                  rib::Event::select_slot(4 + column), true);
         }
         return {};
      case RIB_KEY_DOWN:
         if (focus.pause_row() < 0)
         {
            int slot = focus.pause_action().slot;
            if (slot <= 3)
               focus_pause(rib::Event::select_slot(slot + 3), false);
            else
            {
               char ids[16][64];
               const int count = pause_row(ids, 16);
               focus_pause_row(
                     slot - 4 < count ? slot - 4 : count - 1, false);
            }
         }
         else
         {
            const int column = focus.pause_row() > 2 ? 2 : focus.pause_row();
            focus.pause_row(-1);
            focus_pause(
                  rib::Event::select_slot(1 + column), false);
         }
         return {};
      case RIB_KEY_LEFT:
         if (focus.pause_row() < 0)
         {
            int slot = focus.pause_action().slot;
            int row_start = slot <= 3 ? 1 : 4;
            slot = slot == row_start ? row_start + 2 : slot - 1;
            focus_pause(
                  rib::Event::select_slot(slot), true);
         }
         else
         {
            char ids[16][64];
            const int count = pause_row(ids, 16);
            focus_pause_row(
                  pause_row_index((const char (*)[64])ids, count) - 1, true);
         }
         return {};
      case RIB_KEY_RIGHT:
         if (focus.pause_row() < 0)
         {
            int slot = focus.pause_action().slot;
            int row_end = slot <= 3 ? 3 : rib::kSlotCount;
            slot = slot == row_end ? row_end - 2 : slot + 1;
            focus_pause(
                  rib::Event::select_slot(slot), false);
         }
         else
         {
            char ids[16][64];
            const int count = pause_row(ids, 16);
            focus_pause_row(
                  pause_row_index((const char (*)[64])ids, count) + 1, false);
         }
         return {};
      case RIB_KEY_OK:
      case RIB_KEY_SELECT:
         if (focus.pause_row() < 0)
            return focus.pause_action();
         else
         {
            /* Press the element itself, so a button from the design opens what
             * its listener opens, without its name in this code. */
            char ids[16][64];
            const int count = pause_row(ids, 16);
            const int index =
                  pause_row_index((const char (*)[64])ids, count);
            if (count > 0)
               document.click_element(ids[index]);
         }
         return {};
      case RIB_KEY_CANCEL:
      case RIB_KEY_RESUME:
      case RIB_KEY_TOGGLE:
         return RIB_RMLUI_ACTION_RESUME;
      case RIB_KEY_START:
         return RIB_RMLUI_ACTION_SAVE;
      default:
         return {};
   }
}
}
