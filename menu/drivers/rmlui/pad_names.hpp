#pragma once

#include <map>
#include <string>

namespace rib {
struct HotkeyBinding;

/* The words we show for a pad input, on HOTKEYS and on CONTROLS. For a
 * position of the standard pad, or Home, they are its name in the profile of
 * the first player's pad, or else the standard word for it, which we export
 * beside the defaults of HOTKEYS (pad_word_<id>). For any other input they
 * are its number, as Button 13 or Axis +3 (words.inc). */
class PadNames
{
public:
   /* Read the standard words from the menu folder `assets`. */
   void load(const char *assets);
   /* The words for the position or Home `id`. */
   std::string position(const std::string& id) const;
   /* The words for a pad input in the form of RetroArch's config: "13",
    * "h0up" or "+3". */
   std::string input(const std::string& value) const;
   /* The words for a binding to a hotkey: the word for its key, or the
    * words for its pad inputs joined as a chord. */
   std::string binding(const HotkeyBinding& binding) const;

private:
   std::map<std::string, std::string> standard;
};
}
