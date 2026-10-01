#pragma once

#include "hotkey_bindings.hpp"
#include "words.hpp"

namespace rib {
class Overlays;
class Slots;

/* The hotkeys for use during play (hotkeys.inc, InGame). With QUICK SAVE and
 * QUICK LOAD the player saves to and loads the slot selected in the menu, as
 * with SAVE and LOAD there. With PREVIOUS SLOT and NEXT SLOT the player
 * changes that selection, from the last slot to the first and back. We act
 * once per press and report the result in the notice row. */
class PlayHotkeys
{
public:
   PlayHotkeys(const HotkeyBindings& bindings, Slots& slots, Overlays& overlays)
      : bindings(bindings), slots(slots), overlays(overlays) {}
   /* Call once a frame. `playing` is false while the menu is open or the game
    * waits. We then ignore the hotkeys, and a press that started then counts
    * only after release and a new press. We report a finished save or load
    * from a hotkey whether or not the game is playing. */
   void frame(bool playing);

private:
   void act(Hotkey hotkey);
   /* Show `word` about `slot` in the notice row. */
   void notice(Word word, int slot);
   const HotkeyBindings& bindings;
   Slots& slots;
   Overlays& overlays;
   /* Which hotkeys the player pressed in the previous frame. */
   bool held[kHotkeyCount] = {};
};
}
