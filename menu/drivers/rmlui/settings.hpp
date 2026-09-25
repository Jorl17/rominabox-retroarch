#ifndef RIB_MENU_SETTINGS_HPP
#define RIB_MENU_SETTINGS_HPP

#include "declarations.h"
#include <retro_miscellaneous.h>

namespace rib {
class Lists;
class Slots;
class Parts;
/* Declared switches that we save, with their shared check of the save slot. */
class Toggles
{
public:
   Toggles(Lists& lists, Slots& slots) : lists(lists), slots(slots) {}
   void load(const rib_design_data& design, const char *data_directory);
   void toggle(const char *id, const char *data_directory);
   void apply() const;
private:
   Lists& lists;
   Slots& slots;
   rib_toggle_t entries[RIB_TOGGLE_MAX]{};
   int count = 0;
};

/* The settings that the player changes in the Options of the game, as we
 * declare them in the exporter. Each controls one RetroArch key. We apply a
 * change at once through the host, save it in a file in the data folder of
 * the game, and apply that file in the launcher at the next launch. The
 * value is in RetroArch, and here we keep the declarations and show them. */
class PlayerSettings
{
public:
   PlayerSettings(Parts& parts, Lists& lists) : parts(parts), lists(lists) {}
   void load(const rib_design_data& design, const char *data_directory);
   /* Call once the document is loaded. We set the step of each level, and move
    * a level between two positions onto one. */
   void attach();
   void paint() const;
   /* The player moved the slider `control` to `fraction`. We apply each new
    * position once, with its sound. Pass `persist` when the move is finished
    * (a key, an arrow, a released drag) to save it. False if not a setting. */
   bool slide(const char *control, float fraction, bool persist);
   bool toggle(const char *control);
private:
   const rib_setting_declaration *owning(const char *control,
         enum rib_setting_kind kind) const;
   float value(const rib_setting_declaration& setting) const;
   void set(const rib_setting_declaration& setting, float value, bool persist);
   Parts& parts;
   Lists& lists;
   rib_setting_declaration entries[RIB_SETTING_MAX]{};
   int count = 0;
   char data[PATH_MAX_LENGTH]{};
};
}
#endif
