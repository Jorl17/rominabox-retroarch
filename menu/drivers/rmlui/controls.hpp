#pragma once

#include "declarations.h"
#include <retro_miscellaneous.h>
#include <stddef.h>
#include <stdint.h>

struct config_file;
/* Export limits. They are sizes of buffers, not a list of controls. */
enum { RIB_CONTROL_MAX = 48, RIB_DEVICE_MAX = 8, RIB_CONTROL_CAPTURE_SECONDS = 10 };

namespace rib {
struct Control
{
   char id[32]{};
   char group[32]{};
   unsigned bind_index = 0;
};

/* Binding configuration, capture, the controller picker and the timed
 * binding list. The runtime commands are in host.h. */
class Controls
{
public:
   int index_of(const char *id) const;
   bool active(int index) const;
   int first() const;
   int step(int current, int direction) const;
   bool load_file(const char *path, bool defaults);
   void refresh();
   void focus(int index);
   void cancel_capture(const char *status);
   void start_capture(int index);
   void poll_capture();
   void reset_defaults();
   void choose_device(const char *chosen);
   void toggle_picker();
   void configure_binds(const rib_design_data& design);
   void update_binds(int x, int y, bool pointer_active, bool hover_active);

   bool visible = false;
   bool loaded = false;
   bool capture_active = false;
   int capture_control = 0;
   bool capture_ignore_pointer = false;
   int focused = 0;
   char profile_id[32]{};
   char device_ids[RIB_DEVICE_MAX][32]{};
   char device_names[RIB_DEVICE_MAX][NAME_MAX_LENGTH]{};
   unsigned device_libretro[RIB_DEVICE_MAX]{};
   int device_count = 0;
   bool device_picker_open = false;
   Control entries[RIB_CONTROL_MAX];
   int count = 0;
   bool enabled[RIB_CONTROL_MAX]{};
   char path[PATH_MAX_LENGTH]{};
   char labels[RIB_CONTROL_MAX][NAME_MAX_LENGTH]{};

private:
   void discover_controls(config_file *config);
   void discover_devices(config_file *config);
   void reload();
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
      int after_ms = 0, width = 0;
      int control = -1;
      int64_t since = 0;
      bool open = false;
   } binds;
};
}
