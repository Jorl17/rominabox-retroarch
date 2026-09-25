#pragma once

#include <string>

namespace rib {
/* The role of a screen that we treat specially in the menu, from the roles
 * declared in document_contract.inc. We read the word for the role of a
 * screen from design.cfg once, when the design loads. */
enum class ScreenRole
{
   None,
#define RIB_ROLE(name, word) name,
#include "document_contract.inc"
};

/* The role with the name `word`, or None. */
inline ScreenRole screen_role(const std::string& word)
{
   static const struct { const char *word; ScreenRole role; } roles[] = {
#define RIB_ROLE(name, word) {word, ScreenRole::name},
#include "document_contract.inc"
   };
   for (const auto& known : roles)
      if (word == known.word)
         return known.role;
   return ScreenRole::None;
}

/* The word for `role` in design.cfg, or empty for None. */
inline const char *role_word(ScreenRole role)
{
   switch (role)
   {
#define RIB_ROLE(name, word) case ScreenRole::name: return word;
#include "document_contract.inc"
      case ScreenRole::None: break;
   }
   return "";
}
}
