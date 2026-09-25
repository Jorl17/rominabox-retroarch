#ifndef RIB_MENU_SHADERS_HPP
#define RIB_MENU_SHADERS_HPP

#include "declarations.h"
#include "list_owner.hpp"
#include <string>

namespace rib {
class Lists;

/* The exported shader list and how we show its current row. In Menu we pass
 * it the assets of the export and the data folder of the game on load. */
class Shaders : public ListOwner
{
public:
   Shaders(Lists& lists) : lists(lists) {}
   void load(const char *asset_directory, const char *data_directory);
   void show_running() const;
   ScreenRole role() const override { return ScreenRole::Shaders; }
   /* Apply the shader in the row and remember it for the next launch. */
   bool choose(const char *row) override;
private:
   Lists& lists;
   rib_shader_catalog catalog{};
   std::string assets, data;
};
}
#endif
