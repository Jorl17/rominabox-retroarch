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
   FocusTarget step(int direction) const;
   bool load_file(const char *path, bool defaults);
   void refresh();
   void focus(FocusTarget target);
   void cancel_capture(const char *status);
   void start_capture(int index);
   void poll_capture();
   void reset_defaults();
   void choose_device(const char *chosen);
   void toggle_picker();
   void configure_binds(const rib_design_data& design);
   void update_binds(int x, int y, bool pointer_active, bool hover_active);

   bool loaded = false;
   bool capture_active = false;
   int capture_control = 0;
   bool capture_ignore_pointer = false;
   char profile_id[32]{};
   bool device_picker_open = false;
   rib_controls_catalog catalog{};
   char path[PATH_MAX_LENGTH]{};
   char defaults_path[PATH_MAX_LENGTH]{};

private:
   Focus& focus_state;
   Screens& screens;
   Document& document;
   ControlView& control_view;
   Lists& lists;
   Status& status;
   const Event& hovered;
   char exported_profile[32]{};
   bool apply(const char *wanted, bool player_file);
   bool read_defaults(const char *wanted);
   bool read_player_file();
   std::string player_profile();
   void show_pad();
   bool save();
   const char *console_name(int index) const;
   int find_conflict(int changed_index) const;
   int bind_members(int index, int *members) const;
   bool same_bind_target(int left, int right) const;
   void bind_anchor(int index, char *out, size_t length) const;
   void callout_text(int index, char *out, size_t length) const;
   void show_binds(int index);
   void hide_binds();
   struct Binds
   {
      char list[64]{};
      int after_ms = 0, hover_after_ms = 0, width = 0;
      int control = -1;
      int64_t since = 0;
      bool open = false;
   } binds;
};
}
