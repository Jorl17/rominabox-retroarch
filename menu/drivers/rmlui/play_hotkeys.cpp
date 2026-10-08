#include "play_hotkeys.hpp"

#include "host.h"
#include "overlays.hpp"
#include "slots.hpp"
#include <string>

namespace rib {
void PlayHotkeys::notice(Word word, int slot)
{
   overlays.notify({Overlays::Notice::Slot, say(word, {{"slot", std::to_string(slot)}}), "", ""});
}

void PlayHotkeys::act(Hotkey hotkey)
{
   const int slot = slots.selected();
   switch (hotkey)
   {
      case Hotkey::QuickSave:
         /* One transfer at a time, as with SAVE in the menu. A press during a
          * transfer does nothing. */
         if (!slots.transfer_pending())
            slots.request(Slots::Transfer::Save, Slots::Asker::Hotkey);
         break;
      case Hotkey::QuickLoad:
         if (slots.transfer_pending())
            break;
         if (!slots.load_available())
            notice(Word::QuickSlotEmpty, slot);
         else
            slots.request(Slots::Transfer::Load, Slots::Asker::Hotkey);
         break;
      case Hotkey::Fullscreen:
         rib_host_toggle_fullscreen();
         break;
      case Hotkey::PreviousSlot:
      case Hotkey::NextSlot:
      {
         const int step = hotkey == Hotkey::NextSlot ? 1 : kSlotCount - 1;
         slots.set_selected_slot((slot - 1 + step) % kSlotCount + 1);
         notice(Word::QuickSlot, slots.selected());
         break;
      }
      default:
         break;
   }
}

void PlayHotkeys::frame(Doing doing)
{
   const bool playing = doing == Doing::Playing;
   Slots::Finished finished;
   if (slots.take_finished(finished) && finished.asker == Slots::Asker::Hotkey)
   {
      const bool save = finished.kind == Slots::Transfer::Save;
      notice(finished.success ? (save ? Word::QuickSaved : Word::QuickLoaded)
            : (save ? Word::SaveFailed : Word::LoadFailed), finished.slot);
   }
   const Hotkey *hotkeys = all_hotkeys();
   for (size_t index = 0; index < kHotkeyCount; ++index)
   {
      const Hotkey hotkey = hotkeys[index];
      if (hotkey_acts(hotkey) == Acts::InMenu)
         continue;
      /* While the player types, we note the keys held but act only on a pad. */
      const bool pressed = bindings.held(hotkey, doing != Doing::Typing) && !held[index];
      held[index] = bindings.held(hotkey, true);
      /* The player also uses MENU in the menu, and we open and close the
       * menu for it elsewhere. */
      if (pressed && doing != Doing::Capturing
            && (playing || hotkey_acts(hotkey) == Acts::Both))
         act(hotkey);
   }
   was_playing = playing;
}

bool PlayHotkeys::holding(Hotkey hotkey) const
{
   return was_playing && bindings.offered(hotkey) && held[static_cast<size_t>(hotkey)];
}
}
