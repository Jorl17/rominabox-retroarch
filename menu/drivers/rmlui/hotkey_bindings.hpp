#pragma once

#include <cstddef>
#include <map>
#include <string>
#include <vector>

namespace rib {
/* The hotkeys, as declared in hotkeys.inc. */
enum class Hotkey
{
#define RIB_HOTKEY(name, id, keeps, acts) name,
#include "hotkeys.inc"
};

inline constexpr size_t kHotkeyCount = 0
#define RIB_HOTKEY(name, id, keeps, acts) + 1
#include "hotkeys.inc"
      ;

/* Every hotkey, in declaration order. */
const Hotkey *all_hotkeys();
/* The id of `hotkey` in design.json, in the files and in the document. */
const char *hotkey_id(Hotkey hotkey);
/* Return the hotkey named `id`, or false when there is none. */
bool hotkey_named(const std::string& id, Hotkey& hotkey);

/* One binding to a hotkey: a key, or pad inputs pressed together. */
struct HotkeyBinding
{
   enum class Kind { Key, Pad };
   Kind kind = Kind::Key;
   /* The name of a key in the RetroArch config, or the id of each pad input. */
   std::string key;
   std::vector<std::string> pads;
   /* For the host: the code of a key, or the bind of each pad input. */
   unsigned code = 0;
   std::vector<unsigned> binds;
   /* The binding in the format of hotkeys.inc. */
   std::string text() const;
   bool operator==(const HotkeyBinding& other) const { return text() == other.text(); }
};

/* The inputs of one control of the game: the code of its key (0 for none),
 * and the id of its pad position, with the label we show for it on CONTROLS.
 * A hotkey for use during play may have none of these. */
struct GameInput
{
   std::string label;
   unsigned key = 0;
   std::string position;
};

/* Read a binding in the format of hotkeys.inc through the host. Returns
 * false for text that is not a binding we can read through the host. */
bool read_hotkey_binding(const std::string& text, HotkeyBinding& binding);
/* The key that cancels a capture. We never capture this key. */
bool cancels_capture(const HotkeyBinding& binding);

/* The bindings of each hotkey, from the menu defaults of the author and,
 * once the player has changed any, from the data folder of the game. We keep
 * the rules of hotkeys.inc in each change. An input has one hotkey, or two
 * that may share it, and no hotkey loses a binding that it must keep. */
class HotkeyBindings
{
public:
   /* Read the bindings of the author and the words for pad inputs from
    * `assets`, then those of the player from `data`. When the file of the
    * player breaks the rules, we use the author bindings and say so. */
   void load(const char *assets, const char *data);
   const std::vector<HotkeyBinding>& of(Hotkey hotkey) const;
   /* The words we show in the menu for `binding`: the word for a key
    * (key_words.inc), or the default words for its pad inputs, in order. */
   std::string words(const HotkeyBinding& binding) const;

   enum class Outcome
   {
      /* Nothing changed, because the hotkey already had the binding. */
      Unchanged,
      Added,
      /* We took it from `other` and gave `other` the bindings of that kind
       * that we took from this hotkey, so it keeps one of that kind. */
      Swapped,
      /* We took it from `other`, which still has others of that kind, or need
       * not keep any. */
      Moved,
      Removed,
      /* We refuse because the row would have more bindings than fit on it. */
      Full,
      /* We refuse because `other` would have no binding left, or no key. */
      NeedsBinding,
      NeedsKey,
      /* We refuse because `other`, a hotkey for use during play, would get an
       * input of `control`, that is a key or a single pad button. A chord of
       * several inputs, and Home, are never inputs of the game. */
      GameInput,
   };
   struct Change
   {
      Outcome outcome = Outcome::Unchanged;
      /* The hotkey we took the binding from, or refused the change for. */
      Hotkey other = Hotkey::Menu;
      /* The control of the game in a GameInput refusal, as named on CONTROLS. */
      std::string control;
      /* Whether we saved the change in the file of the player. */
      bool saved = true;
   };
   /* Give `binding` to `hotkey` and take it from any hotkey that may not share
    * it. `room` is how many bindings fit on the row of each hotkey, and `game`
    * contains the current inputs of the game. */
   Change add(Hotkey hotkey, const HotkeyBinding& binding, const std::vector<size_t>& room,
         const std::vector<GameInput>& game);
   Change remove(Hotkey hotkey, size_t index);
   /* Go back to the bindings of the author and delete the file of the player.
    * Returns false when we could not delete the file. */
   bool reset();

   /* Each frame, for the RetroArch input code: whether a binding of `hotkey`
    * is pressed. A shared input counts only for the first hotkey of its pair.
    * `keys` is true when the keyboard counts. */
   bool held(Hotkey hotkey, bool keys) const;
   /* The codes of every key bound to `hotkey`. */
   std::vector<unsigned> keys(Hotkey hotkey) const;
   /* The binds of every pad input bound to a hotkey for use in the menu. */
   std::vector<unsigned> menu_pad_binds() const;
   /* Whether the key `code` is bound to a hotkey for use in the menu. */
   bool menu_key(unsigned code) const;
   /* Ignore the inputs of `binding` that are pressed now until each is
    * released. The player is still pressing the input that ended a capture,
    * and it would otherwise trigger what we just bound it to. */
   void until_released(const HotkeyBinding& binding);

private:
   bool save() const;
   bool keeps(Hotkey hotkey, const std::vector<HotkeyBinding>& list, Outcome *refusal) const;
   bool lawful(const std::vector<HotkeyBinding> (&lists)[kHotkeyCount]) const;
   bool counts_for(Hotkey hotkey, const HotkeyBinding& binding) const;
   /* Whether the player is still pressing any input of `binding` that we wait
    * for. We stop waiting for the inputs released since the last call. */
   bool waiting(const HotkeyBinding& binding) const;
   mutable std::vector<unsigned> unreleased_keys, unreleased_pads;
   std::vector<HotkeyBinding> lists[kHotkeyCount];
   std::vector<HotkeyBinding> authored[kHotkeyCount];
   std::map<std::string, std::string> pad_words;
   std::string player_path;
};

/* When a hotkey works, as declared in hotkeys.inc: while the menu is open,
 * during play, or both. */
enum class Acts { InMenu, InGame, Both };
Acts hotkey_acts(Hotkey hotkey);

/* Whether two hotkeys may share an input, as declared in hotkeys.inc. A
 * shared input triggers `first`. */
bool hotkeys_share(Hotkey one, Hotkey other, Hotkey *first = nullptr);
}
