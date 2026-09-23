#ifndef RIB_MENU_SHADERS_HPP
#define RIB_MENU_SHADERS_HPP

#include <retro_miscellaneous.h>

namespace rib {
/* The exported shader list and how we show its current row. In Menu we pass
 * the selected id and the asset and data paths, already checked. */
class Shaders
{
public:
   void load(const char *asset_directory);
   void show_running() const;
   bool apply(const char *id, const char *assets, const char *data) const;
private:
   enum { Max = 32 };
   char ids[Max][64]{};
   char presets[Max][PATH_MAX_LENGTH]{};
   int count = 0;
   char state_on[32]{};
   char state_off[32]{};
};
}
#endif
