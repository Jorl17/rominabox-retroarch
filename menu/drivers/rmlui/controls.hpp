#pragma once

#include "declarations.h"
#include "focus.hpp"
#include "screens.hpp"
#include <retro_miscellaneous.h>
#include <stddef.h>
#include <stdint.h>
#include <string>

struct config_file;
/* Export limits. They are sizes of buffers, not a list of controls. */
enum { RIB_CONTROL_CAPTURE_SECONDS = 10 };

namespace rib {
class Document;
class ControlView;
class Lists;
class Status;
/* Binding configuration, capture, the controller picker and the timed
 * binding list. The runtime commands are in host.h. */
class Controls
{
public:
   Controls(Focus& focus, Screens& screens, Document& document, ControlView& control_view,
         Lists& lists, Status& status, const Event& hovered)
      : focus_state(focus), screens(screens), document(document), control_view(control_view),
        lists(lists), status(status), hovered(hovered) {}
   int index_of(const char *id) const;
   bool active(int index) const;
   FocusTarget first() const;
   /* Handle an event on the pad screen. During a capture, every event but
    * CANCEL and BACK goes to the capture. The player opens the picker and
    * chooses in it, pressing a control starts a capture, and pressing RESET
    * restores the author's pad. False when the event is not for this screen. */
   bool handle(const Event& event);
   /* A screen has just been shown, this one when `showing`. */
   void screen_shown(bool showing);
   /* While the picker is open, the arrows stay in it, on the chosen pad. When
    * it closes, we focus the button that opens it again. */
   void focus_picker();
   void close_picker();
   /* Read the author's pad from `assets`, then the player's file in `data`,
    * once per menu. */
   void load(const char *assets, const char *data);
   void refresh();
   void focus(FocusTarget target);
   /* End a capture that is waiting, and leave its binding as it was. */
   void cancel_capture();
   /* Capture the binding of `index`, or of each member in turn for a stick. */
   void start_capture(int index);
   void poll_capture();
   void reset_defaults();
   void choose_device(const char *chosen);
   void toggle_picker();
   void configure_binds(const BindsDeclaration& binds);
   /* The list of the bindings of a control, as declared in the design. */
   const char *binds_list() const { return binds.list.c_str(); }
   void update_binds(int x, int y, bool pointer_active);

   bool loaded = false;
   bool capture_active = false;
   /* The control waiting for input now, or for a stick, the member we capture. */
   int capture_control = 0;
   bool capture_ignore_pointer = false;
   /* The pad applied now. */
   std::string profile_id;
   bool device_picker_open = false;
   rib_controls_catalog catalog{};

private:
   Focus& focus_state;
   Screens& screens;
   Document& document;
   ControlView& control_view;
   Lists& lists;
   Status& status;
   const Event& hovered;
   /* Where the author's defaults and scenes are, and the player's file. */
   std::string assets, defaults_path, path;
   /* The pad the game was exported with. */
   std::string exported_profile;
   /* What we capture, in the order from bind_members: for a stick, its members
    * up, right, down, left and then the click, and any other control alone.
    * capture_members[capture_step] is capture_control. */
   int capture_members[RIB_CONTROL_MAX] = {};
   int capture_count = 0;
   int capture_step = 0;
   /* The first control that a new binding in this capture clashed with,
    * which we report when the capture ends, or -1 for none. */
   int capture_clash = -1;
   /* Wait in the host for input for `index`, with its name on the status
    * line. False when the host cannot capture it. */
   bool capture_member(int index);
   /* End the capture, say on the status line how it ended, and put back the
    * footer of the screen. */
   void end_capture(const std::string& words);
   bool apply(const char *wanted, bool player_file);
   /* Apply the emulated device of pad `id` to the core, and write it to the
    * remap we read at the next launch. */
   void apply_device(const std::string& id);
   bool read_defaults(const char *wanted);
   bool read_player_file();
   std::string player_profile();
   void show_pad();
   bool save();
   const char *console_name(int index) const;
   int find_conflict(int changed_index) const;
   int bind_members(int index, int *members) const;
   bool same_bind_target(int left, int right) const;
   /* The element next to which we show the list of bindings of a control. */
   std::string bind_anchor(int index) const;
   /* Append the host lines for one binding, each key in its words. */
   void bind_lines(unsigned bind_index, char details[][64], char kinds[][8], int *lines) const;
   /* The bindings of a control, as we show them on its callout. */
   std::string callout_text(int index) const;
   void show_binds(int index);
   void hide_binds();
   struct Binds
   {
      std::string list;
      int after_ms = 0, hover_after_ms = 0, width = 0;
      int control = -1;
      int64_t since = 0;
      bool open = false;
   } binds;
};
}
