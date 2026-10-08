#pragma once

#include "hotkey_bindings.hpp"
#include "words.hpp"

namespace rib {
class Overlays;
class Slots;

/* The hotkeys for use during play (hotkeys.inc, InGame and Both). With QUICK
 * SAVE and QUICK LOAD the player saves to and loads the slot selected in the
 * menu, as with SAVE and LOAD there. With PREVIOUS SLOT and NEXT SLOT the
 * player changes that selection, from the last slot to the first and back.
 * With FULLSCREEN the player switches between fullscreen and a window, in
 * the menu too. We act once per press and report the result of a save, a
 * load or a change of slot in the notice row. */
class PlayHotkeys
{
public:
   /* What the player is doing in a frame. While the game plays, we act on
    * every hotkey here. While the menu is open, or while we keep the game
    * waiting behind an overlay, we act only on those that the player also
    * uses in the menu, and while the player types into a field, only on their
    * pad bindings. While the player chooses a binding, we give every press to
    * the capture and act on no hotkey. */
   enum class Doing { Playing, Waiting, Typing, Capturing };
   PlayHotkeys(const HotkeyBindings& bindings, Slots& slots, Overlays& overlays)
      : bindings(bindings), slots(slots), overlays(overlays) {}
   /* Call once a frame. A press that started while we could not act on its
    * hotkey counts only after release and a new press. We report a finished
    * save or load from a hotkey whatever the player is doing. */
   void frame(Doing doing);
   /* Whether `hotkey` was held during play in the last frame, which we read
    * for a hotkey that works only while it is held. */
   bool holding(Hotkey hotkey) const;

private:
   void act(Hotkey hotkey);
   /* Show `word` about `slot` in the notice row. */
   void notice(Word word, int slot);
   const HotkeyBindings& bindings;
   Slots& slots;
   Overlays& overlays;
   /* For each hotkey, whether it was held in the frame before, and whether
    * the game was running then. */
   bool held[kHotkeyCount] = {};
   bool was_playing = false;
};
}
