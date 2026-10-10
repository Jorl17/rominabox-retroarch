#pragma once

#include <climits>
#include <map>
#include <set>
#include <string>

namespace rib {
struct HotkeyBinding;

/* The words we show for a pad input, on HOTKEYS, on CONTROLS and for a
 * binding (data-binding). For a position of the standard pad, or Home, they
 * are its name in the profile of the pad in use, or else the standard word
 * for it, which we export beside the defaults of HOTKEYS (pad_word_<id>).
 * The positions in pad_words_by_place always have the standard word. For any
 * other input they are its number, as Button 13 or Axis +3 (words.inc). */
class PadNames
{
public:
   /* Read the standard words from the menu folder `assets`. */
   void load(const char *assets);
   /* Name inputs in the profile of the pad at joypad index `pad`, the pad in
    * use (inputs.hpp). Until we call it, the first player's pad. */
   void use_pad(unsigned pad) { this->pad = pad; }
   /* The words for the position or Home `id`. */
   std::string position(const std::string& id) const;
   /* The words for a pad input in the form of RetroArch's config: "13",
    * "h0up" or "+3". */
   std::string input(const std::string& value) const;
   /* The words for a binding to a hotkey: the word for its key, or the
    * words for its pad inputs joined as a chord. */
   std::string binding(const HotkeyBinding& binding) const;
   /* The words for a binding where the menu has little room, in a hint or on
    * a badge: a key by its short word (key_words.inc). */
   std::string short_binding(const HotkeyBinding& binding) const;

private:
   /* The words for the pad inputs of `binding`, joined as a chord. */
   std::string pads(const HotkeyBinding& binding) const;
   std::map<std::string, std::string> standard;
   std::set<std::string> by_place;
   unsigned pad = UINT_MAX;
};
}
