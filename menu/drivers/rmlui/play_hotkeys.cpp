#include "play_hotkeys.hpp"

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

void PlayHotkeys::frame(bool playing)
{
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
      if (hotkey_acts(hotkey) != Acts::InGame)
         continue;
      const bool now = bindings.held(hotkey, true);
      const bool pressed = now && !held[index];
      held[index] = now;
      if (pressed && playing)
         act(hotkey);
   }
}
}
