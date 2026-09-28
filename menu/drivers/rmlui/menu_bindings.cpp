#include "menu_bindings.hpp"

#include "declarations.h"
#include "files.h"
#include "host.h"
#include "words.hpp"
#include "../../../verbosity.h"
#include <file/config_file.h>
#include <file/file_path.h>
#include <streams/file_stream.h>
#include <string/stdstring.h>
#include <algorithm>
#include <cstring>

namespace rib {
namespace {
enum class Keeps { Binding, Key };

struct Declared
{
   MenuAction action;
   const char *id;
   Keeps keeps;
};

const Declared declared[] = {
#define RIB_MENU_ACTION(name, id, keeps) {MenuAction::name, id, Keeps::keeps},
#include "menu_controls.inc"
};

const MenuAction all[] = {
#define RIB_MENU_ACTION(name, id, keeps) MenuAction::name,
#include "menu_controls.inc"
};

struct Sharing
{
   MenuAction first, second;
};

const Sharing sharing[] = {
#define RIB_MENU_ACTIONS_SHARE(first, second) {MenuAction::first, MenuAction::second},
#include "menu_controls.inc"
};

#define RIB_MENU_BINDING(name, prefix) const std::string name##_prefix = prefix;
#define RIB_MENU_PAD_CHORD(separator) const std::string chord = separator;
#define RIB_MENU_CAPTURE_CANCEL(key) const char cancel_key[] = key;
#include "menu_controls.inc"

size_t at(MenuAction action) { return static_cast<size_t>(action); }

/* The ids in a space-separated list, in order. */
std::vector<std::string> split(const std::string& list, const std::string& separator)
{
   std::vector<std::string> found;
   size_t from = 0;
   while (from <= list.size())
   {
      const size_t end = list.find(separator, from);
      const std::string part = list.substr(from, end == std::string::npos ? std::string::npos : end - from);
      if (!part.empty())
         found.push_back(part);
      if (end == std::string::npos)
         break;
      from = end + separator.size();
   }
   return found;
}

/* Read the list of one action from a file, or return false when the file has
 * none. We leave out, and report, bindings we cannot read through the host. */
bool read_list(config_file_t *config, MenuAction action, const char *path,
      std::vector<MenuBinding>& out)
{
   const struct config_entry_list *entry =
         config_get_entry(config, keys::MenuControl(menu_action_id(action)).c_str());
   if (!entry || !entry->value)
      return false;
   out.clear();
   for (const std::string& text : split(entry->value, " "))
   {
      MenuBinding binding;
      if (read_menu_binding(text, binding))
         out.push_back(binding);
      else
         RARCH_ERR("[RIB] %s binds %s to '%s', which is no key and no pad input "
               "this player reads; it is left out.\n", path, menu_action_id(action), text.c_str());
   }
   return true;
}

bool is_key(const MenuBinding& binding) { return binding.kind == MenuBinding::Kind::Key; }

bool holds(const std::vector<MenuBinding>& list, const MenuBinding& binding)
{
   return std::find(list.begin(), list.end(), binding) != list.end();
}
}

const MenuAction *menu_actions() { return all; }

const char *menu_action_id(MenuAction action) { return declared[at(action)].id; }

bool menu_action_named(const std::string& id, MenuAction& action)
{
   for (const Declared& one : declared)
      if (id == one.id)
      {
         action = one.action;
         return true;
      }
   return false;
}

bool menu_actions_share(MenuAction one, MenuAction other, MenuAction *first)
{
   for (const Sharing& pair : sharing)
      if ((pair.first == one && pair.second == other) || (pair.first == other && pair.second == one))
      {
         if (first)
            *first = pair.first;
         return true;
      }
   return false;
}

std::string MenuBinding::text() const
{
   if (kind == Kind::Key)
      return Key_prefix + key;
   std::string text = Pad_prefix;
   for (size_t index = 0; index < pads.size(); ++index)
      text += (index ? chord : std::string()) + pads[index];
   return text;
}

bool read_menu_binding(const std::string& text, MenuBinding& binding)
{
   binding = MenuBinding{};
   if (text.compare(0, Key_prefix.size(), Key_prefix) == 0)
   {
      binding.kind = MenuBinding::Kind::Key;
      binding.key = text.substr(Key_prefix.size());
      return !binding.key.empty() && rib_host_key_code(binding.key.c_str(), &binding.code);
   }
   if (text.compare(0, Pad_prefix.size(), Pad_prefix) != 0)
      return false;
   binding.kind = MenuBinding::Kind::Pad;
   binding.pads = split(text.substr(Pad_prefix.size()), chord);
   for (const std::string& id : binding.pads)
   {
      unsigned bind = 0;
      if (!rib_host_pad_input(id.c_str(), &bind)
            || std::find(binding.binds.begin(), binding.binds.end(), bind) != binding.binds.end())
         return false;
      binding.binds.push_back(bind);
   }
   return !binding.binds.empty();
}

bool cancels_capture(const MenuBinding& binding)
{
   return is_key(binding) && string_is_equal_noncase(binding.key.c_str(), cancel_key);
}

void MenuBindings::load(const char *assets, const char *data)
{
   const std::string defaults = std::string(assets ? assets : "") + "/" + files::MenuControlsDefaults;
   player_path = data && *data ? std::string(data) + "/" + files::MenuControls : std::string();
   for (size_t index = 0; index < kMenuActionCount; ++index)
      authored[index].clear();
   pad_words.clear();
   if (config_file_t *config = config_file_new_from_path_to_string(defaults.c_str()))
   {
      for (MenuAction action : all)
         read_list(config, action, defaults.c_str(), authored[at(action)]);
      const std::string word = keys::PadWord("");
      struct config_file_entry entry;
      for (bool present = config_get_entry_list_head(config, &entry); present;
            present = config_get_entry_list_next(&entry))
         if (entry.key && entry.value && !std::strncmp(entry.key, word.c_str(), word.size()))
            pad_words[entry.key + word.size()] = entry.value;
      config_file_free(config);
   }
   for (size_t index = 0; index < kMenuActionCount; ++index)
      lists[index] = authored[index];
   if (player_path.empty() || !path_is_valid(player_path.c_str()))
      return;
   config_file_t *config = config_file_new_from_path_to_string(player_path.c_str());
   if (!config)
      return;
   std::vector<MenuBinding> chosen[kMenuActionCount];
   bool any = false;
   for (MenuAction action : all)
   {
      if (read_list(config, action, player_path.c_str(), chosen[at(action)]))
         any = true;
      else
         chosen[at(action)] = authored[at(action)];
   }
   config_file_free(config);
   if (!any)
      return;
   if (!lawful(chosen))
   {
      RARCH_ERR("[RIB] %s would leave an action of the menu with nothing it must "
            "keep, or give one input to two actions; the game's own bindings are used.\n",
            player_path.c_str());
      return;
   }
   for (size_t index = 0; index < kMenuActionCount; ++index)
      lists[index] = chosen[index];
}

const std::vector<MenuBinding>& MenuBindings::of(MenuAction action) const
{
   return lists[at(action)];
}

std::string MenuBindings::words(const MenuBinding& binding) const
{
   if (is_key(binding))
      return key_word(binding.key.c_str());
   std::string text;
   for (size_t index = 0; index < binding.pads.size(); ++index)
   {
      const auto word = pad_words.find(binding.pads[index]);
      text += (index ? chord : std::string())
            + (word != pad_words.end() ? word->second : binding.pads[index]);
   }
   return text;
}

bool MenuBindings::keeps(MenuAction action, const std::vector<MenuBinding>& list,
      Outcome *refusal) const
{
   if (declared[at(action)].keeps == Keeps::Key
         && std::none_of(list.begin(), list.end(), is_key))
   {
      if (refusal) *refusal = Outcome::NeedsKey;
      return false;
   }
   if (list.empty())
   {
      if (refusal) *refusal = Outcome::NeedsBinding;
      return false;
   }
   return true;
}

bool MenuBindings::lawful(const std::vector<MenuBinding> (&candidate)[kMenuActionCount]) const
{
   for (MenuAction action : all)
   {
      if (!keeps(action, candidate[at(action)], nullptr))
         return false;
      for (MenuAction other : all)
         if (other != action && !menu_actions_share(action, other))
            for (const MenuBinding& binding : candidate[at(action)])
               if (holds(candidate[at(other)], binding))
                  return false;
   }
   return true;
}

MenuBindings::Change MenuBindings::add(MenuAction action, const MenuBinding& binding,
      const std::vector<size_t>& room)
{
   Change change;
   change.other = action;
   if (holds(lists[at(action)], binding))
      return change;
   std::vector<MenuBinding> next[kMenuActionCount];
   for (size_t index = 0; index < kMenuActionCount; ++index)
      next[index] = lists[index];
   std::vector<MenuBinding>& mine = next[at(action)];
   bool taken = false, swapped = false;
   for (MenuAction other : all)
   {
      if (other == action || menu_actions_share(action, other)
            || !holds(lists[at(other)], binding))
         continue;
      std::vector<MenuBinding>& theirs = next[at(other)];
      theirs.erase(std::remove(theirs.begin(), theirs.end(), binding), theirs.end());
      if (!taken)
         change.other = other;
      taken = true;
      if (std::any_of(theirs.begin(), theirs.end(),
               [&](const MenuBinding& kept) { return kept.kind == binding.kind; }))
         continue;
      /* When it has none of that kind left, we give it those of that kind
       * that we take from this action, but never an input of a third
       * action that it may not share. */
      for (const MenuBinding& giving : lists[at(action)])
      {
         const bool held_elsewhere = std::any_of(all, all + kMenuActionCount,
               [&](MenuAction third) {
                  return third != action && third != other
                        && !menu_actions_share(third, other) && holds(lists[at(third)], giving);
               });
         if (giving.kind != binding.kind || held_elsewhere || holds(theirs, giving))
            continue;
         theirs.push_back(giving);
         mine.erase(std::remove(mine.begin(), mine.end(), giving), mine.end());
         swapped = true;
      }
   }
   mine.push_back(binding);
   for (MenuAction each : all)
   {
      Outcome refusal = Outcome::Unchanged;
      if (!keeps(each, next[at(each)], &refusal))
      {
         change.outcome = refusal;
         change.other = each;
         return change;
      }
      if (at(each) < room.size() && next[at(each)].size() > room[at(each)])
      {
         change.outcome = Outcome::Full;
         change.other = each;
         return change;
      }
   }
   for (size_t index = 0; index < kMenuActionCount; ++index)
      lists[index] = next[index];
   change.outcome = swapped ? Outcome::Swapped : taken ? Outcome::Moved : Outcome::Added;
   change.saved = save();
   return change;
}

MenuBindings::Change MenuBindings::remove(MenuAction action, size_t index)
{
   Change change;
   change.other = action;
   std::vector<MenuBinding>& list = lists[at(action)];
   if (index >= list.size())
      return change;
   std::vector<MenuBinding> left = list;
   left.erase(left.begin() + (std::ptrdiff_t)index);
   if (!keeps(action, left, &change.outcome))
      return change;
   list = left;
   change.outcome = Outcome::Removed;
   change.saved = save();
   return change;
}

bool MenuBindings::reset()
{
   for (size_t index = 0; index < kMenuActionCount; ++index)
      lists[index] = authored[index];
   return player_path.empty() || !path_is_valid(player_path.c_str())
         || filestream_delete(player_path.c_str()) == 0;
}

bool MenuBindings::save() const
{
   if (player_path.empty())
      return false;
   config_file_t *config = config_file_new_alloc();
   if (!config)
      return false;
   for (MenuAction action : all)
   {
      std::string list;
      for (const MenuBinding& binding : lists[at(action)])
         list += (list.empty() ? "" : " ") + binding.text();
      config_set_string(config, keys::MenuControl(menu_action_id(action)).c_str(), list.c_str());
   }
   const bool saved = rib_write_menu_config(config, player_path.c_str());
   config_file_free(config);
   return saved;
}

bool MenuBindings::counts_for(MenuAction action, const MenuBinding& binding) const
{
   MenuAction first = action;
   for (MenuAction other : all)
      if (other != action && menu_actions_share(action, other, &first) && first != action
            && holds(lists[at(other)], binding))
         return false;
   return true;
}

bool MenuBindings::held(MenuAction action, bool keys) const
{
   for (const MenuBinding& binding : lists[at(action)])
   {
      if (!counts_for(action, binding))
         continue;
      if (is_key(binding))
      {
         if (keys && rib_host_key_down(binding.code))
            return true;
         continue;
      }
      if (std::all_of(binding.binds.begin(), binding.binds.end(), rib_host_pad_down))
         return true;
   }
   return false;
}

std::vector<unsigned> MenuBindings::keys(MenuAction action) const
{
   std::vector<unsigned> codes;
   for (const MenuBinding& binding : lists[at(action)])
      if (is_key(binding) && counts_for(action, binding))
         codes.push_back(binding.code);
   return codes;
}

std::vector<unsigned> MenuBindings::pad_binds() const
{
   std::vector<unsigned> binds;
   for (const auto& list : lists)
      for (const MenuBinding& binding : list)
         for (unsigned bind : binding.binds)
            if (std::find(binds.begin(), binds.end(), bind) == binds.end())
               binds.push_back(bind);
   return binds;
}
}
