#include "setting_display.hpp"

#include "document_contract.hpp"
#include "elements.hpp"
#include "words.hpp"

#include <algorithm>
#include <cmath>
#include <string>

namespace rib {
int level_last_position(const SettingDeclaration& level)
{
   return (int)level.values.size() - 1;
}

int level_position_of(const SettingDeclaration& level, float value)
{
   int nearest = 0;
   for (int position = 1; position <= level_last_position(level); ++position)
      if (std::fabs(level.values[position] - value)
            <= std::fabs(level.values[nearest] - value))
         nearest = position;
   return nearest;
}

int level_position_at(const SettingDeclaration& level, float fraction)
{
   fraction = std::min(1.0f, std::max(0.0f, fraction));
   return (int)(fraction * (float)level_last_position(level) + 0.5f);
}

float level_fraction_at(const SettingDeclaration& level, int position)
{
   return (float)position / (float)level_last_position(level);
}

bool switch_on(const SettingDeclaration& setting, float value)
{
   return (value != 0.0f) != setting.inverted;
}

void draw_slider(Rml::Element *slider, float fraction, SliderPainted& painted)
{
   if (!slider)
      return;
   fraction = std::min(1.0f, std::max(0.0f, fraction));
   if (auto *context = slider->GetContext())
      context->Update();
   auto *track = find_class(slider, document_contract::SliderTrack);
   auto *fill = find_class(slider, document_contract::SliderFill);
   auto *thumb = find_class(slider, document_contract::SliderThumb);
   const float width = track ? track->GetBox().GetSize(Rml::BoxArea::Content).x : 0.0f;
   const float thumb_width = thumb ? thumb->GetBox().GetSize(Rml::BoxArea::Border).x : 0.0f;
   /* We measure in pixels, which change with the window and with the layout
    * of the track, so we run this whenever those may have changed and write
    * only what differs from the last write. */
   if (width <= 0.0f)
      return;
   const float filled = width * fraction;
   const float left = std::max(0.0f, width - thumb_width) * fraction;
   if (fill && filled != painted.fill)
      fill->SetProperty("width", std::to_string(filled) + "px");
   if (thumb && left != painted.left)
      thumb->SetProperty("left", std::to_string(left) + "px");
   painted.fill = filled;
   painted.left = left;
}

std::string level_readout(const SettingDeclaration& level, float value)
{
   switch (level.key)
   {
#define RIB_SETTING_PERCENT(name) \
      case RIB_SETTING_##name: return std::to_string((long)std::lround(value * 100.0f)) + "%";
#include "settings.inc"
      default:
         return std::string();
   }
}

void paint_setting(Rml::Element *root, const SettingDeclaration& setting, float value,
      bool used, SliderPainter& sliders)
{
   if (!root)
      return;
   Rml::Element *control = root->GetElementById(setting.control);
   disable(control, !used);
   if (setting.kind == SettingKind::Level)
   {
      const int position = level_position_of(setting, value);
      if (control && control->IsClassSet(document_contract::Slider))
         sliders.paint_slider(control, level_fraction_at(setting, position),
               level_readout(setting, setting.values.empty() ? value : setting.values[position]));
      return;
   }
   Rml::Element *state = root->GetElementById(setting.control + document_contract::StateSuffix);
   if (setting.kind == SettingKind::Choice)
   {
      /* The word for its position, <id>-<n> from 1, or the value when no
       * word is declared in the player. */
      const int position = level_position_of(setting, value);
      Word word;
      write_text(state, word_named(setting.id + "-" + std::to_string(position + 1), word)
            ? say(word) : std::to_string(setting.values[position]));
      return;
   }
   const bool on = switch_on(setting, value);
   if (control)
      control->SetClass(document_contract::On, on);
   write_text(state, say(on ? Word::SwitchOn : Word::SwitchOff));
}
}
