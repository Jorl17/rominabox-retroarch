#pragma once

#include <cstring>

namespace rib {
/* The role of a screen that we treat specially in the menu. It is a word
 * declared in design.cfg, and we read it once, here. */
enum class ScreenRole { None, Pause, Options, Controls, Shaders, Achievements, Discs, Accounts };

inline ScreenRole screen_role(const char *word)
{
   static const struct { const char *word; ScreenRole role; } roles[] = {
      {"pause", ScreenRole::Pause}, {"options", ScreenRole::Options},
      {"controls", ScreenRole::Controls}, {"shaders", ScreenRole::Shaders},
      {"achievements", ScreenRole::Achievements}, {"discs", ScreenRole::Discs},
      {"accounts", ScreenRole::Accounts},
   };
   for (const auto& known : roles)
      if (word && !std::strcmp(word, known.word))
         return known.role;
   return ScreenRole::None;
}
}
