#pragma once

#include "declarations.h"

#include <RmlUi/Core.h>

namespace rib {
/* How we draw a player setting, in the menu and in the offscreen preview of
 * its pictures: the positions and slider of a level, the state of a switch,
 * and as disabled when the setting has no effect in the game. */

/* The positions of a level count from 0 at its low end, and the value at each
 * is values[position]. We convert between a position and a value or a slider
 * fraction here and nowhere else. */
int level_last_position(const SettingDeclaration& level);
/* The position with the value nearest `value`, or the higher of two ties. */
int level_position_of(const SettingDeclaration& level, float value);
/* The position of a slider at `fraction` of its track. */
int level_position_at(const SettingDeclaration& level, float fraction);
float level_fraction_at(const SettingDeclaration& level, int position);

/* Whether a switch is on at `value`, a value of the key that may mean off
 * when the switch shows on (`inverted`). */
bool switch_on(const SettingDeclaration& setting, float value);

/* The value of a setting when RetroArch has none: a level at its high end,
 * or false for the key of a switch. */
float unreported_value(const SettingDeclaration& setting);

/* How we draw the slider of a level at a fraction of its track. In the menu
 * we keep the position of each slider for the keys and pointer that move it,
 * and in the preview we only draw. */
class SliderPainter
{
public:
   virtual ~SliderPainter() = default;
   virtual void paint_slider(Rml::Element *slider, float fraction) = 0;
};

/* The last values we set for the fill and thumb of a slider, in pixels, so
 * that we write again only what differs. */
struct SliderPainted
{
   float fill = -1.0f;
   float left = -1.0f;
};

/* Set the fill and thumb of `slider` at `fraction` of its track. We measure
 * from the box of the track, so we update the styles first, and we write
 * nothing while the track has no width. */
void draw_slider(Rml::Element *slider, float fraction, SliderPainted& painted);

/* Show `setting`, under `root`, at `value`: the slider of a level through
 * `sliders`, the `on` fact of a switch and its word in <control>-state, and
 * disabled when there is no `use` for it in the game. */
void paint_setting(Rml::Element *root, const SettingDeclaration& setting, float value,
      bool used, SliderPainter& sliders);
}
