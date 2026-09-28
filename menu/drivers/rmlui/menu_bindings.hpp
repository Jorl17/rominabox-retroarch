#pragma once

#include <cstddef>
#include <map>
#include <string>
#include <vector>

namespace rib {
/* The actions of the menu, as declared in menu_controls.inc. */
enum class MenuAction
{
#define RIB_MENU_ACTION(name, id, keeps) name,
#include "menu_controls.inc"
};

inline constexpr size_t kMenuActionCount = 0
#define RIB_MENU_ACTION(name, id, keeps) + 1
#include "menu_controls.inc"
      ;

/* Every action, in declaration order. */
const MenuAction *menu_actions();
/* The id of `action` in design.json, in the files and in the document. */
const char *menu_action_id(MenuAction action);
/* Return the action named `id`, or false when there is none. */
bool menu_action_named(const std::string& id, MenuAction& action);

/* One binding to a menu action: a key, or pad inputs pressed together. */
struct MenuBinding
{
   enum class Kind { Key, Pad };
   Kind kind = Kind::Key;
   /* The name of a key in the RetroArch config, or the id of each pad input. */
   std::string key;
   std::vector<std::string> pads;
   /* For the host: the code of a key, or the bind of each pad input. */
   unsigned code = 0;
   std::vector<unsigned> binds;
   /* The binding in the format of menu_controls.inc. */
   std::string text() const;
   bool operator==(const MenuBinding& other) const { return text() == other.text(); }
};

/* Read a binding in the format of menu_controls.inc through the host.
 * Returns false for text that is not a binding we can read through the host. */
bool read_menu_binding(const std::string& text, MenuBinding& binding);
/* The key that cancels a capture. We never capture this key. */
bool cancels_capture(const MenuBinding& binding);

/* The bindings of each action, from the menu defaults of the author and,
 * once the player has changed any, from the data folder of the game. We keep
 * the rules of menu_controls.inc in each change. An input has one action, or
 * two that may share it, and no action loses a binding that it must keep. */
class MenuBindings
{
public:
   /* Read the bindings of the author and the words for pad inputs from
    * `assets`, then those of the player from `data`. When the file of the
    * player breaks the rules, we use the author bindings and say so. */
   void load(const char *assets, const char *data);
   const std::vector<MenuBinding>& of(MenuAction action) const;
   /* The words we show in the menu for `binding`: the word for a key
    * (key_words.inc), or the default words for its pad inputs, in order. */
   std::string words(const MenuBinding& binding) const;

   enum class Outcome
   {
      /* Nothing changed, because the action already had the binding. */
      Unchanged,
      Added,
      /* We took it from `other` and gave `other` the bindings of that kind
       * that we took from this action, so it keeps one of that kind. */
      Swapped,
      /* We took it from `other`, which still has others of that kind. */
      Moved,
      Removed,
      /* We refuse because the row would have more bindings than fit on it. */
      Full,
      /* We refuse because `other` would have no binding left, or no key. */
      NeedsBinding,
      NeedsKey,
   };
   struct Change
   {
      Outcome outcome = Outcome::Unchanged;
      /* The action we took the binding from, or refused the change for. */
      MenuAction other = MenuAction::Menu;
      /* Whether we saved the change in the file of the player. */
      bool saved = true;
   };
   /* Give `binding` to `action` and take it from any action that may not share
   * it. `room` is how many bindings fit on the row of each action. */
   Change add(MenuAction action, const MenuBinding& binding, const std::vector<size_t>& room);
   Change remove(MenuAction action, size_t index);
   /* Go back to the bindings of the author and delete the file of the player.
    * Returns false when we could not delete the file. */
   bool reset();

   /* Each frame, for the RetroArch input code: whether a binding of `action`
    * is pressed. A shared input counts only for the first action of its pair.
    * `keys` is true when the keyboard counts. */
   bool held(MenuAction action, bool keys) const;
   /* The codes of every key bound to `action`. */
   std::vector<unsigned> keys(MenuAction action) const;
   /* The binds of every pad input bound to any action. */
   std::vector<unsigned> pad_binds() const;
   /* Ignore the inputs of `binding` that are pressed now until each is
    * released. The player is still pressing the input that ended a capture,
    * and it would otherwise trigger what we just bound it to. */
   void until_released(const MenuBinding& binding);

private:
   bool save() const;
   bool keeps(MenuAction action, const std::vector<MenuBinding>& list, Outcome *refusal) const;
   bool lawful(const std::vector<MenuBinding> (&lists)[kMenuActionCount]) const;
   bool counts_for(MenuAction action, const MenuBinding& binding) const;
   /* Whether the player is still pressing any input of `binding` that we wait
    * for. We stop waiting for the inputs released since the last call. */
   bool waiting(const MenuBinding& binding) const;
   mutable std::vector<unsigned> unreleased_keys, unreleased_pads;
   std::vector<MenuBinding> lists[kMenuActionCount];
   std::vector<MenuBinding> authored[kMenuActionCount];
   std::map<std::string, std::string> pad_words;
   std::string player_path;
};

/* Whether two actions may share an input, as declared in menu_controls.inc.
 * A shared input triggers `first`. */
bool menu_actions_share(MenuAction one, MenuAction other, MenuAction *first = nullptr);
}
