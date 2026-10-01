#include "words.hpp"
#include "controls.hpp"
#include "host.h"
#include "files.h"
#include "document.hpp"
#include "control_view.hpp"
#include "declarations.h"
#include "lists.hpp"
#include "status.hpp"
#include "sounds.hpp"
#include "elements.hpp"
#include "../../../verbosity.h"
#include <file/config_file.h>
#include <streams/file_stream.h>
#include <string/stdstring.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace rib {
int Controls::index_of(const char *id) const
{
   for (int index = 0; id && index < catalog.count; ++index)
      if (catalog.entries[index].id == id) return index;
   return -1;
}

bool Controls::active(int index) const
{
   return index >= 0 && index < catalog.count &&
          catalog.entries[index].enabled;
}

const char * Controls::console_name(int index) const
{
   if (!catalog.entries[index].label.empty())
      return catalog.entries[index].label.c_str();
   return catalog.entries[index].id.c_str();
}

std::vector<GameInput> Controls::game_inputs() const
{
   std::vector<GameInput> inputs;
   for (int index = 0; index < catalog.count; ++index)
   {
      if (!active(index))
         continue;
      GameInput input;
      input.label = console_name(index);
      rib_host_bind_key(catalog.entries[index].bind_index, &input.key);
      input.position = catalog.entries[index].slot;
      inputs.push_back(input);
   }
   return inputs;
}

FocusTarget Controls::first() const
{
   for (int index = 0; index < catalog.count; ++index)
      if (active(index)) return FocusTarget::item(index);
   return FocusTarget::item(0);
}

namespace {
/* The element for a stop on the pad screen. The controls of a stick share the
 * box of their group, and any other control is its callout. */
std::string stop_id(const rib_controls_catalog& catalog, FocusTarget target)
{
   switch (target.kind)
   {
      case FocusTarget::Kind::Reset: return document_contract::ControlsReset;
      case FocusTarget::Kind::Back: return document_contract::ControlsBack;
      case FocusTarget::Kind::Item: break;
   }
   const auto& entry = catalog.entries[target.index];
   if (!entry.group.empty())
      return document_contract::ControlGroupPrefix + entry.group;
   return document_contract::ControlPrefix + entry.id;
}

/* The focused stop in RmlUi, as a stop of the pad screen, with an item index
 * of -1 when it is none of them. For a stick, its first member. */
FocusTarget focused_stop(const rib_controls_catalog& catalog, const std::string& id)
{
   if (id == document_contract::ControlsReset) return FocusTarget::reset();
   if (id == document_contract::ControlsBack) return FocusTarget::back();
   for (int index = 0; index < catalog.count; ++index)
      if (catalog.entries[index].enabled && stop_id(catalog, FocusTarget::item(index)) == id)
         return FocusTarget::item(index);
   return FocusTarget::item(-1);
}
}

/* At startup we read the author's defaults, then the player's file. When the
 * player chose another pad, we use that pad. Otherwise we only put the
 * player's labels and bindings over the author's. */
void Controls::load(const char *assets, const char *data)
{
   this->assets = assets ? assets : "";
   defaults_path = this->assets + "/" + files::ControlsDefaults;
   path = data && *data ? std::string(data) + "/" + files::Controls : std::string();
   if (!apply(NULL, false))
      RARCH_WARN("[RmlUi] Controls defaults not found at %s.\n", defaults_path.c_str());
   if (path.empty())
      return;
   const std::string chosen = player_profile();
   if (!chosen.empty() && chosen != profile_id)
      apply(chosen.c_str(), true);
   else
   {
      read_player_file();
      show_pad();
   }
}

/* The one way we apply a pad: its controls, labels and bindings from the
 * author's defaults, then the player's file unless the player pressed Reset,
 * and then the picture and the name in the picker. `wanted` is a pad the
 * player chose. With NULL we keep the pad in the player's file, or else the
 * author's. */
bool Controls::apply(const char *wanted, bool player_file)
{
   const std::string chosen = !wanted && player_file ? player_profile() : std::string();
   if (!chosen.empty())
      wanted = chosen.c_str();
   if (!read_defaults(wanted))
      return false;
   if (player_file)
      read_player_file();
   show_pad();
   return true;
}

bool Controls::read_defaults(const char *wanted)
{
   config_file_t *config;
   bool present = false;
   int index;

   /* We find the author's pad when we open the defaults. For a pad that this
    * game does not offer, we use the author's pad instead. */
   std::string exported;
   if (!(config = rib_open_controls(defaults_path.c_str(), true, exported,
               &catalog, &present, rib_host_bind_index)))
      return false;
   exported_profile = exported;
   if (!wanted || !*wanted
         || !rib_controls_discover(config, wanted, &catalog, rib_host_bind_index))
      wanted = exported_profile.c_str();
   profile_id = wanted;
   rib_controls_read_enabled(config, &catalog);
   for (index = 0; index < catalog.count; ++index)
   {
      if (!active(index))
         continue;
      catalog.entries[index].label.clear();
      rib_host_clear_bind(catalog.entries[index].bind_index);
      rib_controls_read_label(config, &catalog.entries[index]);
      rib_host_load_bind(config, catalog.entries[index].slot.c_str(), catalog.entries[index].bind_index);
   }
   rib_host_restore_keyboard_mapping();
   config_file_free(config);
   return true;
}

/* The pad in the player's file, empty when there is none. */
std::string Controls::player_profile()
{
   std::string named;
   bool present = false;
   if (!path.empty())
      if (config_file_t *config = rib_open_controls(path.c_str(), false, named, &catalog,
               &present, rib_host_bind_index))
         config_file_free(config);
   return named;
}

bool Controls::read_player_file()
{
   std::string named;
   bool present = false;
   config_file_t *config;
   int index;

   if (path.empty() || !(config = rib_open_controls(path.c_str(), false, named, &catalog,
               &present, rib_host_bind_index)))
      return false;
   for (index = 0; index < catalog.count; ++index)
   {
      if (!active(index))
         continue;
      rib_controls_read_label(config, &catalog.entries[index]);
      rib_host_load_bind(config, catalog.entries[index].slot.c_str(), catalog.entries[index].bind_index);
   }
   rib_host_restore_keyboard_mapping();
   config_file_free(config);
   return true;
}

/* Draw the pad we apply and show its name in the picker. The exported
 * document already contains the author's pad, and we read any other pad from
 * its scene in the export. */
void Controls::show_pad()
{
   const std::string drawn = *control_view.scene_profile()
         ? control_view.scene_profile() : exported_profile;
   if (drawn != profile_id)
   {
      const std::string scene = assets + "/" + files::Scene(profile_id);
      int64_t length = 0;
      void *markup = NULL;
      if (filestream_read_file(scene.c_str(), &markup, &length) && markup
            && control_view.set_scene(profile_id.c_str(), (const char*)markup))
         RARCH_LOG("[RIB] drawing '%s' from %s.\n", profile_id.c_str(), scene.c_str());
      else
         RARCH_ERR("[RIB] no scene for '%s' at %s; the pad on screen "
               "is still the one the game was exported with.\n", profile_id.c_str(), scene.c_str());
      free(markup);
   }
   control_view.set_device_picker(catalog, device_picker_open, profile_id.c_str());
}

bool Controls::save()
{
   config_file_t *config;
   bool saved;
   int index;

   if (path.empty() || !(config = config_file_new_alloc()))
      return false;

   config_set_string(config, keys::ControlsProfile, profile_id.c_str());
   for (index = 0; index < catalog.count; ++index)
   {
      if (!active(index))
         continue;
      const rib_control_declaration& control = catalog.entries[index];
      config_set_string(config, keys::ControlLabel(control.id).c_str(), control.label.c_str());
      rib_host_write_bind(config, control.slot.c_str(), control.bind_index);
   }

   saved = rib_write_menu_config(config, path.c_str());
   config_file_free(config);
   return saved;
}

void Controls::refresh()
{
   int index;
   /* The stop being captured, which is the focused one, is marked capturing.
    * All members of a stick use the box of their group. In that box, only
    * the mark of the member waiting now is marked capturing. */
   const std::string captured = capture_active
         ? stop_id(catalog, FocusTarget::item(capture_control)) : std::string();
   for (index = 0; index < catalog.count; ++index)
   {
      if (!active(index))
         continue;
      const std::string binding = callout_text(index);
      const std::string stop = stop_id(catalog, FocusTarget::item(index));
      control_view.set_control_state(stop.c_str(), catalog.entries[index].id.c_str(),
            catalog.entries[index].label.c_str(), binding.c_str(), stop == captured);
      if (!catalog.entries[index].group.empty())
      {
         control_view.set_member_capturing(catalog.entries[index].id.c_str(),
               capture_active && index == capture_control);
         document.set_element_text((document_contract::ControlGroupBindingPrefix
               + catalog.entries[index].group).c_str(), binding.c_str());
      }
   }
   control_view.set_capturing(capture_active);
}

void Controls::end_capture(const std::string& words)
{
   capture_active = false;
   this->status.set_controls(words.c_str());
   screens.restore_footer();
   refresh();
}

/* Members captured before CANCEL keep their new binding, because we saved
 * each one when it finished, and for a stick stopped partway we say so. */
void Controls::cancel_capture()
{
   if (!capture_active)
      return;
   rib_host_capture_cancel();
   end_capture(say(capture_step > 0 ? Word::BindingSaved : Word::BindingUnchanged));
}

bool Controls::capture_member(int index)
{
   if (!rib_host_capture_start(catalog.entries[index].bind_index,
            RIB_CONTROL_CAPTURE_SECONDS))
      return false;
   capture_active = true;
   capture_control = index;
   /* The gesture that started the capture, or finished the previous member,
    * is not input for this member. */
   pointer.start();
   this->status.set_controls(say(Word::CaptureCountdown, {{"control", console_name(index)},
         {"seconds", std::to_string(RIB_CONTROL_CAPTURE_SECONDS)}}).c_str());
   refresh();
   return true;
}

void Controls::start_capture(int index)
{
   if (!active(index))
      return;
   capture_count = bind_members(index, capture_members);
   capture_step = 0;
   capture_clash = -1;
   if (!capture_member(capture_members[0]))
   {
      this->status.set_controls(say(Word::CaptureFailed).c_str());
      return;
   }
   screens.set_footer_hint(say(Word::CancelHint).c_str());
}

int Controls::find_conflict(int changed_index) const
{
   int index;
   if (!active(changed_index))
      return -1;
   for (index = 0; index < catalog.count; ++index)
   {
      if (index == changed_index || !active(index))
         continue;
      if (rib_host_bind_conflicts(catalog.entries[changed_index].bind_index,
               catalog.entries[index].bind_index))
         return index;
   }
   return -1;
}

bool Controls::same_bind_target(int left, int right) const
{
   if (left == right)
      return true;
   if (left < 0 || right < 0
         || left >= catalog.count || right >= catalog.count)
      return false;
   const std::string& group_left = catalog.entries[left].group;
   return !group_left.empty() && group_left == catalog.entries[right].group;
}

std::string Controls::bind_anchor(int index) const
{
   if (!catalog.entries[index].group.empty())
   {
      const std::string group = document_contract::ControlGroupPrefix + catalog.entries[index].group;
      if (document.has_element(group.c_str()))
         return group;
   }
   return document_contract::ControlPrefix + catalog.entries[index].id;
}

void Controls::bind_lines(unsigned bind_index, char details[][64], char kinds[][8], int *lines) const
{
   const int before = *lines;
   rib_host_bind_lines(bind_index, details, kinds, lines);
   for (int line = before; line < *lines; ++line)
      if (std::strcmp(kinds[line], "KEY") == 0)
         strlcpy(details[line], key_word(details[line]).c_str(), sizeof(details[line]));
}

std::string Controls::callout_text(int index) const
{
   int members[RIB_CONTROL_MAX];
   char details[RIB_HOST_BIND_LINE_MAX][64];
   char kinds[RIB_HOST_BIND_LINE_MAX][8];
   int lines = 0;

   if (index < 0 || index >= catalog.count)
      return say(Word::Unbound);
   const int member_count = bind_members(index, members);
   for (int member = 0; member < member_count; ++member)
      bind_lines(catalog.entries[members[member]].bind_index, details, kinds, &lines);
   if (lines <= 0)
      return say(Word::Unbound);
   std::string text;
   for (int line = 0; line < lines; ++line)
      text += (line ? ", " : "") + std::string(details[line]);
   return text;
}

void Controls::show_binds(int index)
{
   int members[RIB_CONTROL_MAX];
   int member_count = 0;
   char details[RIB_HOST_BIND_LINE_MAX][64];
   char kinds[RIB_HOST_BIND_LINE_MAX][8];
   std::string titles[RIB_HOST_BIND_LINE_MAX];
   int lines = 0;
   int rows;
   int slot;
   int member;

   member_count = bind_members(index, members);

   for (member = 0; member < member_count; ++member)
   {
      int before = lines;
      const unsigned at = catalog.entries[members[member]].bind_index;
      bind_lines(at,
            details, kinds, &lines);
      for (slot = before; slot < lines; ++slot)
         titles[slot] = console_name(members[member]);
   }

   /* With one binding, the callout already shows it. A list that repeated it
    * would open on every control as the player moves across the pad. */
   if (lines < 2)
   {
      hide_binds();
      return;
   }

   rows = lists.rows_in(binds.list.c_str());
   if (lines > rows)
      RARCH_ERR("[RIB] '%s' has %d binds and the menu was built with %d rows; "
            "the rest are not shown.\n",
            catalog.entries[index].id.c_str(), lines, rows);
   for (slot = 0; slot < rows; ++slot)
   {
      const std::string row = lists.row_in(binds.list.c_str(), slot);
      if (row.empty())
         break;
      if (slot < lines)
         lists.set_row_text(row.c_str(), titles[slot].c_str(), details[slot], kinds[slot]);
      document.set_shown(row.c_str(), slot < lines);
   }
   lists.retarget_pages(binds.list.c_str());
   lists.place_list(binds.list.c_str(), bind_anchor(index).c_str(), binds.width);
   binds.open = true;
}

void Controls::hide_binds()
{
   if (!binds.list.empty())
      document.set_shown(binds.list.c_str(), false);
   binds.open = false;
}

void Controls::focus(FocusTarget target)
{
   if (target.kind == FocusTarget::Kind::Item && !active(target.index)) return;
   focus_state.set(stop_id(catalog, target).c_str());
}

void Controls::update_binds(int x, int y, bool pointer_active)
{
   int current = -1;
   int64_t now;

   if (binds.list.empty() || !screens.showing(ScreenRole::Controls)
         || capture_active || device_picker_open)
   {
      hide_binds();
      binds.control = -1;
      return;
   }

   if (pointer_active
         && document.pointer_inside(binds.list.c_str(), x, y)
         && binds.control >= 0)
      current = binds.control;
   const auto target = focused_stop(catalog, focus_state.current_id());
   const int focused = target.kind == FocusTarget::Kind::Item ? target.index : -1;
   if (current < 0 && focused >= 0
         && focused < catalog.count)
      current = focused;
   if (!active(current))
      current = -1;

   if (!same_bind_target(current, binds.control))
   {
      hide_binds();
      binds.control = current;
      binds.since = rib_host_time_us();
   }
   if (current < 0 || binds.open)
      return;
   /* We open the list sooner for a pointer resting on the control than for a
    * key press that only passed through it on the way elsewhere. */
   const int after_ms = this->hovered.kind == RIB_RMLUI_ACTION_CONTROL
         && index_of(this->hovered.id.c_str()) == current
         ? binds.hover_after_ms : binds.after_ms;
   now = rib_host_time_us();
   if (now - binds.since >= (int64_t)after_ms * 1000)
      show_binds(current);
}

/* We apply it to the core now, not at the next launch. The emulated device
 * is a setting in RetroArch, and we apply it again with
 * CMD_EVENT_CONTROLLER_INIT, as for any other change of device in the
 * frontend. A catalog device of 0 means "no subclass". The default in the
 * frontend is then a joypad, and writing 0 would connect nothing. At the next
 * launch we read the device from the remap and not from the per-game override. */
void Controls::apply_device(const std::string& id)
{
   for (int index = 0; index < catalog.device_count; ++index)
      if (catalog.devices[index].id == id)
      {
         rib_host_apply_device(id.c_str(), catalog.devices[index].libretro);
         return;
      }
}

void Controls::choose_device(const char *chosen)
{
   device_picker_open = false;
   if (chosen && *chosen && profile_id != chosen)
   {
      profile_id = chosen;
      /* The pad belongs to the player who picks it, so we write it to the
       * per-game override and never to the author's fixed defaults. */
      save();

      apply_device(chosen);

      if (!apply(chosen, true))
         RARCH_ERR("[RIB] could not re-read controls from %s after changing "
               "controller; the menu still lists the previous pad.\n",
               defaults_path.c_str());
      focus(first());
      refresh();
   }
   control_view.set_device_picker(catalog, false, profile_id.c_str());
}

void Controls::reset_defaults()
{
   if (capture_active)
      cancel_capture();
   device_picker_open = false;
   if (!apply(NULL, false))
   {
      this->status.set_controls(say(Word::DefaultsLoadFailed).c_str());
      refresh();
      return;
   }
   /* The author's pad, and so the device for it, because the player may
    * have picked another pad. */
   apply_device(profile_id);
   if (!save())
      this->status.set_controls(say(Word::DefaultsSaveFailed).c_str());
   else
      this->status.set_controls(say(Word::DefaultsRestored).c_str());
   refresh();
}

void Controls::poll_capture()
{
   if (capture_active)
   {
      float remaining = 0.0f;
      enum rib_capture_result result = rib_host_capture_poll(
            pointer.counts(), &remaining);
      if (result == RIB_CAPTURE_CAPTURED)
      {
         /* We save each member when we capture it, and for a stick we go on
          * to the next member. When a save fails, we stop there and say so. */
         const int conflict = find_conflict(capture_control);
         if (capture_clash < 0)
            capture_clash = conflict;
         rib_host_restore_keyboard_mapping();
         const bool saved = save();
         if (saved && ++capture_step < capture_count)
         {
            if (!capture_member(capture_members[capture_step]))
               end_capture(say(Word::CaptureFailed));
         }
         else if (!saved)
            end_capture(say(Word::BindingSaveFailed));
         else if (capture_clash >= 0)
            end_capture(say(Word::BindingConflict, {{"control", console_name(capture_clash)}}));
         else
            end_capture(say(Word::BindingSaved));
      }
      else if (result == RIB_CAPTURE_TIMED_OUT)
         end_capture(say(capture_step > 0 ? Word::BindingSaved : Word::CaptureTimeout));
      else
         this->status.set_controls(say(Word::CaptureCountdown,
               {{"control", console_name(capture_control)},
                {"seconds", std::to_string((unsigned)(remaining + 0.999f))}}).c_str());
   }

}

void Controls::configure_binds(const BindsDeclaration& declared)
{
   binds.list = declared.list;
   binds.after_ms = declared.after_ms;
   binds.hover_after_ms = declared.hover_after_ms;
   binds.width = declared.width;
}

void Controls::toggle_picker()
{
   device_picker_open = !device_picker_open;
   control_view.set_device_picker(catalog, device_picker_open, profile_id.c_str());
}

void Controls::close_picker()
{
   if (device_picker_open)
      toggle_picker();
}

void Controls::focus_picker()
{
   Rml::Element *box = document.root()
         ? document.root()->GetElementById(document_contract::ControlsDevice) : nullptr;
   if (device_picker_open && box)
   {
      focus_state.trap(box);
      Rml::Element *chosen = nullptr;
      walk(box, [&](Rml::Element *option) {
         if (option->IsClassSet(document_contract::Selected)
               && option->GetId().rfind(document_contract::ControlsDeviceOptionPrefix, 0) == 0)
         {
            chosen = option;
            return Walk::Stop;
         }
         return Walk::Continue;
      });
      if (!focus_state.set(chosen))
         focus_state.set(focus_state.first(
               document.root()->GetElementById(document_contract::ControlsDeviceList)));
      return;
   }
   focus_state.trap(nullptr);
   focus_state.set(document_contract::ControlsDeviceCurrent);
}

bool Controls::handle(const Event& event)
{
   switch (event.kind)
   {
      case RIB_RMLUI_ACTION_CONTROLS_CANCEL:
         play_action_sound(event.kind);
         cancel_capture();
         return true;
      case RIB_RMLUI_ACTION_CONTROLS_BACK:
         /* Leaving the pad screen ends a capture. We choose the next
          * screen in the menu code. */
         cancel_capture();
         return false;
      default:
         break;
   }
   /* While we capture a binding, the player cannot press anything else. */
   if (capture_active)
      return true;
   switch (event.kind)
   {
      case RIB_RMLUI_ACTION_DEVICE_PICKER_TOGGLE:
         toggle_picker();
         focus_picker();
         return true;
      case RIB_RMLUI_ACTION_DEVICE_PICKER_CHOOSE:
         choose_device(event.id.c_str());
         focus_picker();
         return true;
      case RIB_RMLUI_ACTION_CONTROL:
      {
         const int index = index_of(event.id.c_str());
         if (active(index))
         {
            play_action_sound(event.kind);
            focus(FocusTarget::item(index));
            start_capture(index);
         }
         return true;
      }
      case RIB_RMLUI_ACTION_CONTROLS_RESET:
         play_action_sound(event.kind);
         reset_defaults();
         return true;
      default:
         return false;
   }
}

void Controls::screen_shown(bool showing)
{
   if (showing)
   {
      /* On opening, show the text from the design, whatever was there last. */
      status.set_controls("");
      refresh();
   }
   else
      cancel_capture();
}


int Controls::bind_members(int index, int *members) const
{
   if (catalog.entries[index].group.empty())
   {
      members[0] = index;
      return 1;
   }
   int found = 0;
   for (int cursor = 0; cursor < catalog.count; ++cursor)
      if (active(cursor) && catalog.entries[cursor].group == catalog.entries[index].group)
         members[found++] = cursor;
   return found;
}

}
