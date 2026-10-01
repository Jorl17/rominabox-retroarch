#pragma once

#include "declarations.h"

#include <RmlUi/Core.h>
#include <vector>

namespace rib {
/* Show the screen `wanted` under `root`, in the menu and in the offscreen
 * preview of its pictures. Show its panel and hide those of the other
 * declared screens, and set its heading and, where declared, its footer
 * hint. `wanted` is one of `screens`. */
void display_screen(Rml::Element *root, const std::vector<ScreenDeclaration>& screens,
      const ScreenDeclaration& wanted);
}
