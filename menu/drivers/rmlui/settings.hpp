#ifndef RIB_MENU_SETTINGS_HPP
#define RIB_MENU_SETTINGS_HPP

#include "declarations.h"
#include <retro_miscellaneous.h>

namespace rib {
/* Declared switches that we save, with their shared check of the save slot. */
class Toggles
{
public:
   void load(const rib_design_data& design, const char *data_directory);
   void toggle(const char *id, const char *data_directory);
   void apply() const;
private:
   rib_toggle_t entries[RIB_TOGGLE_MAX]{};
   int count = 0;
};

/* We manage the runtime volume in the host. Here we keep only the per-game
 * path where we save it, and the shared slider display. */
class Volume
{
public:
   void configure_path(const char *data_directory);
   void initialize();
   void set(float db, bool persist);
   void paint() const;
private:
   char path[PATH_MAX_LENGTH]{};
};
}
#endif
