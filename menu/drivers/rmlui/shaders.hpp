#ifndef RIB_MENU_SHADERS_HPP
#define RIB_MENU_SHADERS_HPP

#include "declarations.h"

namespace rib {
class Lists;

/* The exported shader list and how we show its current row. In Menu we pass
 * the selected id and the asset and data paths, already checked. */
class Shaders
{
public:
   Shaders(Lists& lists) : lists(lists) {}
   void load(const char *asset_directory);
   void show_running() const;
   bool apply(const char *id, const char *assets, const char *data) const;
private:
   Lists& lists;
   rib_shader_catalog catalog{};
};
}
#endif
