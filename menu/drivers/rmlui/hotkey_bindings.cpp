#include "hotkey_bindings.hpp"

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
enum class Keeps { Binding, Key, Nothing };

struct Declared
{
   Hotkey hotkey;
   const char *id;
   Keeps keeps;
   Acts acts;
};

const Declared declared[] = {
#define RIB_HOTKEY(name, id, keeps, acts) {Hotkey::name, id, Keeps::keeps, Acts::acts},
#include "hotkeys.inc"
};

const Hotkey all[] = {
#define RIB_HOTKEY(name, id, keeps, acts) Hotkey::name,
#include "hotkeys.inc"
};

struct Ways
{
   Hotkey hotkey;
   const char *names[2];
};

const Ways ways_declared[] = {
#define RIB_HOTKEY_MODES(name, first, second) {Hotkey::name, {first, second}},
#include "hotkeys.inc"
};

struct Sharing
{
   Hotkey first, second;
};

const Sharing sharing[] = {
#define RIB_HOTKEYS_SHARE(first, second) {Hotkey::first, Hotkey::second},
#include "hotkeys.inc"
};

#define RIB_HOTKEY_BINDING(name, prefix) const std::string name##_prefix = prefix;
#define RIB_HOTKEY_PAD_CHORD(separator) const std::string chord = separator;
#define RIB_HOTKEY_CAPTURE_CANCEL(key) const char cancel_key[] = key;
#include "hotkeys.inc"

size_t at(Hotkey hotkey) { return static_cast<size_t>(hotkey); }

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

/* Read the list of one hotkey from a file, or return false when the file has
 * none. We leave out, and report, bindings we cannot read through the host. */
bool read_list(config_file_t *config, Hotkey hotkey, const char *path,
      std::vector<HotkeyBinding>& out)
{
   const struct config_entry_list *entry =
         config_get_entry(config, keys::HotkeyList(hotkey_id(hotkey)).c_str());
   if (!entry || !entry->value)
      return false;
   out.clear();
   for (const std::string& text : split(entry->value, " "))
   {
      HotkeyBinding binding;
      if (read_hotkey_binding(text, binding))
         out.push_back(binding);
      else
         RARCH_ERR("[RIB] %s binds %s to '%s', which is no key and no pad input "
               "this player reads; it is left out.\n", path, hotkey_id(hotkey), text.c_str());
   }
   return true;
}

bool is_key(const HotkeyBinding& binding) { return binding.kind == HotkeyBinding::Kind::Key; }

bool holds(const std::vector<HotkeyBinding>& list, const HotkeyBinding& binding)
{
   return std::find(list.begin(), list.end(), binding) != list.end();
}

/* Return the input of `game` that `binding` is, if any. That is a single key
 * or a single pad position from the game inputs. */
const GameInput *game_input(const HotkeyBinding& binding, const std::vector<GameInput>& game)
{
   for (const GameInput& input : game)
   {
      if (is_key(binding) ? input.key && input.key == binding.code
            : binding.pads.size() == 1 && binding.pads[0] == input.position)
         return &input;
   }
   return nullptr;
}
}

const Hotkey *all_hotkeys() { return all; }

const char *hotkey_id(Hotkey hotkey) { return declared[at(hotkey)].id; }

bool hotkey_named(const std::string& id, Hotkey& hotkey)
{
   for (const Declared& one : declared)
      if (id == one.id)
      {
         hotkey = one.hotkey;
         return true;
      }
   return false;
}

Acts hotkey_acts(Hotkey hotkey) { return declared[at(hotkey)].acts; }

bool hotkeys_share(Hotkey one, Hotkey other, Hotkey *first)
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

std::string HotkeyBinding::text() const
{
   if (kind == Kind::Key)
      return Key_prefix + key;
   std::string text = Pad_prefix;
   for (size_t index = 0; index < pads.size(); ++index)
      text += (index ? chord : std::string()) + pads[index];
   return text;
}

bool read_hotkey_binding(const std::string& text, HotkeyBinding& binding)
{
   binding = HotkeyBinding{};
   if (text.compare(0, Key_prefix.size(), Key_prefix) == 0)
   {
      binding.kind = HotkeyBinding::Kind::Key;
      binding.key = text.substr(Key_prefix.size());
      return !binding.key.empty() && rib_host_key_code(binding.key.c_str(), &binding.code);
   }
   if (text.compare(0, Pad_prefix.size(), Pad_prefix) != 0)
      return false;
   binding.kind = HotkeyBinding::Kind::Pad;
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

bool cancels_capture(const HotkeyBinding& binding)
{
   return is_key(binding) && string_is_equal_noncase(binding.key.c_str(), cancel_key);
}

void HotkeyBindings::load(const char *assets, const char *data)
{
   const std::string defaults = std::string(assets ? assets : "") + "/" + files::HotkeysDefaults;
   player_path = data && *data ? std::string(data) + "/" + files::Hotkeys : std::string();
   for (size_t index = 0; index < kHotkeyCount; ++index)
      authored[index].clear();
   pad_words.clear();
   if (config_file_t *config = config_file_new_from_path_to_string(defaults.c_str()))
   {
      for (Hotkey hotkey : all)
         read_list(config, hotkey, defaults.c_str(), authored[at(hotkey)]);
      const std::string word = keys::PadWord("");
      struct config_file_entry entry;
      for (bool present = config_get_entry_list_head(config, &entry); present;
            present = config_get_entry_list_next(&entry))
         if (entry.key && entry.value && !std::strncmp(entry.key, word.c_str(), word.size()))
            pad_words[entry.key + word.size()] = entry.value;
      config_file_free(config);
   }
   for (size_t index = 0; index < kHotkeyCount; ++index)
      lists[index] = authored[index];
   read_ways_and_absent(defaults.c_str());
   for (size_t index = 0; index < kHotkeyCount; ++index)
      ways[index] = authored_ways[index];
   if (player_path.empty() || !path_is_valid(player_path.c_str()))
      return;
   config_file_t *config = config_file_new_from_path_to_string(player_path.c_str());
   if (!config)
      return;
   std::vector<HotkeyBinding> chosen[kHotkeyCount];
   bool any = false;
   for (Hotkey hotkey : all)
   {
      if (read_list(config, hotkey, player_path.c_str(), chosen[at(hotkey)]))
         any = true;
      else
         chosen[at(hotkey)] = authored[at(hotkey)];
      /* For a hotkey that the game does not have, we keep no binding from
       * an earlier export. */
      if (absent[at(hotkey)])
         chosen[at(hotkey)].clear();
      const size_t way = read_way(config, hotkey);
      if (way != (size_t)-1)
         ways[at(hotkey)] = way;
   }
   config_file_free(config);
   if (!any)
      return;
   if (!lawful(chosen))
   {
      RARCH_ERR("[RIB] %s would leave a hotkey of the menu with nothing it must "
            "keep, or give one input to two hotkeys; the game's own bindings are used.\n",
            player_path.c_str());
      return;
   }
   for (size_t index = 0; index < kHotkeyCount; ++index)
      lists[index] = chosen[index];
}

const std::vector<HotkeyBinding>& HotkeyBindings::of(Hotkey hotkey) const
{
   return lists[at(hotkey)];
}

std::string HotkeyBindings::words(const HotkeyBinding& binding) const
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

bool HotkeyBindings::keeps(Hotkey hotkey, const std::vector<HotkeyBinding>& list,
      Outcome *refusal) const
{
   const Keeps kept = declared[at(hotkey)].keeps;
   if (kept == Keeps::Nothing)
      return true;
   if (kept == Keeps::Key
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

bool HotkeyBindings::lawful(const std::vector<HotkeyBinding> (&candidate)[kHotkeyCount]) const
{
   for (Hotkey hotkey : all)
   {
      if (!keeps(hotkey, candidate[at(hotkey)], nullptr))
         return false;
      for (Hotkey other : all)
         if (other != hotkey && !hotkeys_share(hotkey, other))
            for (const HotkeyBinding& binding : candidate[at(hotkey)])
               if (holds(candidate[at(other)], binding))
                  return false;
   }
   return true;
}

HotkeyBindings::Change HotkeyBindings::add(Hotkey hotkey, const HotkeyBinding& binding,
      const std::vector<size_t>& room, const std::vector<GameInput>& game)
{
   Change change;
   change.other = hotkey;
   if (holds(lists[at(hotkey)], binding))
      return change;
   std::vector<HotkeyBinding> next[kHotkeyCount];
   for (size_t index = 0; index < kHotkeyCount; ++index)
      next[index] = lists[index];
   std::vector<HotkeyBinding>& mine = next[at(hotkey)];
   bool taken = false, swapped = false;
   for (Hotkey other : all)
   {
      if (other == hotkey || hotkeys_share(hotkey, other)
            || !holds(lists[at(other)], binding))
         continue;
      std::vector<HotkeyBinding>& theirs = next[at(other)];
      theirs.erase(std::remove(theirs.begin(), theirs.end(), binding), theirs.end());
      if (!taken)
         change.other = other;
      taken = true;
      if (declared[at(other)].keeps == Keeps::Nothing
            || std::any_of(theirs.begin(), theirs.end(),
               [&](const HotkeyBinding& kept) { return kept.kind == binding.kind; }))
         continue;
      /* When the other hotkey must keep a binding of that kind and has none
       * left, we give it those of that kind that we take from this hotkey,
       * but never an input of a third hotkey that it may not share. When it
       * need not keep one, we leave it without. */
      for (const HotkeyBinding& giving : lists[at(hotkey)])
      {
         const bool held_elsewhere = std::any_of(all, all + kHotkeyCount,
               [&](Hotkey third) {
                  return third != hotkey && third != other
                        && !hotkeys_share(third, other) && holds(lists[at(third)], giving);
               });
         if (giving.kind != binding.kind || held_elsewhere || holds(theirs, giving))
            continue;
         theirs.push_back(giving);
         mine.erase(std::remove(mine.begin(), mine.end(), giving), mine.end());
         swapped = true;
      }
   }
   mine.push_back(binding);
   /* We give a hotkey for use during play none of the game inputs, whether
    * captured or received in a swap. We do not check what it had before this
    * change. */
   for (Hotkey each : all)
   {
      if (hotkey_acts(each) == Acts::InMenu)
         continue;
      for (const HotkeyBinding& given : next[at(each)])
      {
         const GameInput *input = holds(lists[at(each)], given) ? nullptr : game_input(given, game);
         if (!input)
            continue;
         change.outcome = Outcome::GameInput;
         change.other = each;
         change.control = input->label;
         return change;
      }
   }
   for (Hotkey each : all)
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
   for (size_t index = 0; index < kHotkeyCount; ++index)
      lists[index] = next[index];
   change.outcome = swapped ? Outcome::Swapped : taken ? Outcome::Moved : Outcome::Added;
   change.saved = save();
   return change;
}

HotkeyBindings::Change HotkeyBindings::remove(Hotkey hotkey, size_t index)
{
   Change change;
   change.other = hotkey;
   std::vector<HotkeyBinding>& list = lists[at(hotkey)];
   if (index >= list.size())
      return change;
   std::vector<HotkeyBinding> left = list;
   left.erase(left.begin() + (std::ptrdiff_t)index);
   if (!keeps(hotkey, left, &change.outcome))
      return change;
   list = left;
   change.outcome = Outcome::Removed;
   change.saved = save();
   return change;
}

void HotkeyBindings::read_ways_and_absent(const char *defaults)
{
   for (size_t index = 0; index < kHotkeyCount; ++index)
   {
      authored_ways[index] = 0;
      absent[index] = false;
   }
   config_file_t *config = config_file_new_from_path_to_string(defaults);
   if (!config)
      return;
   for (Hotkey hotkey : all)
   {
      const size_t way = read_way(config, hotkey);
      if (way != (size_t)-1)
         authored_ways[at(hotkey)] = way;
   }
   if (const struct config_entry_list *entry = config_get_entry(config, keys::HotkeysAbsent))
      for (const std::string& id : split(entry->value ? entry->value : "", " "))
      {
         Hotkey hotkey;
         if (hotkey_named(id, hotkey))
            absent[at(hotkey)] = true;
      }
   config_file_free(config);
}

size_t HotkeyBindings::read_way(::config_file *config, Hotkey hotkey) const
{
   const struct config_entry_list *entry =
         config_get_entry(config, keys::HotkeyMode(hotkey_id(hotkey)).c_str());
   if (!entry || !entry->value)
      return (size_t)-1;
   const std::vector<std::string> named = modes(hotkey);
   for (size_t way = 0; way < named.size(); ++way)
      if (named[way] == entry->value)
         return way;
   return (size_t)-1;
}

bool HotkeyBindings::offered(Hotkey hotkey) const
{
   return !absent[at(hotkey)];
}

std::vector<std::string> HotkeyBindings::modes(Hotkey hotkey) const
{
   for (const Ways& declared_ways : ways_declared)
      if (declared_ways.hotkey == hotkey)
         return {declared_ways.names[0], declared_ways.names[1]};
   return {};
}

size_t HotkeyBindings::mode(Hotkey hotkey) const
{
   return ways[at(hotkey)];
}

bool HotkeyBindings::next_mode(Hotkey hotkey)
{
   const size_t count = modes(hotkey).size();
   if (count == 0)
      return true;
   ways[at(hotkey)] = (ways[at(hotkey)] + 1) % count;
   return save();
}

bool HotkeyBindings::reset()
{
   for (size_t index = 0; index < kHotkeyCount; ++index)
   {
      lists[index] = authored[index];
      ways[index] = authored_ways[index];
   }
   return player_path.empty() || !path_is_valid(player_path.c_str())
         || filestream_delete(player_path.c_str()) == 0;
}

bool HotkeyBindings::save() const
{
   if (player_path.empty())
      return false;
   config_file_t *config = config_file_new_alloc();
   if (!config)
      return false;
   for (Hotkey hotkey : all)
   {
      std::string list;
      for (const HotkeyBinding& binding : lists[at(hotkey)])
         list += (list.empty() ? "" : " ") + binding.text();
      config_set_string(config, keys::HotkeyList(hotkey_id(hotkey)).c_str(), list.c_str());
      const std::vector<std::string> named = modes(hotkey);
      if (!named.empty())
         config_set_string(config, keys::HotkeyMode(hotkey_id(hotkey)).c_str(),
               named[ways[at(hotkey)]].c_str());
   }
   const bool saved = rib_write_menu_config(config, player_path.c_str());
   config_file_free(config);
   return saved;
}

bool HotkeyBindings::counts_for(Hotkey hotkey, const HotkeyBinding& binding) const
{
   Hotkey first = hotkey;
   for (Hotkey other : all)
      if (other != hotkey && hotkeys_share(hotkey, other, &first) && first != hotkey
            && holds(lists[at(other)], binding))
         return false;
   return true;
}

void HotkeyBindings::ignore_pressed_until_released()
{
   for (const std::vector<HotkeyBinding>& list : lists)
      for (const HotkeyBinding& binding : list)
      {
         if (is_key(binding))
         {
            if (rib_host_key_down(binding.code))
               unreleased_keys.push_back(binding.code);
            continue;
         }
         for (unsigned bind : binding.binds)
            if (rib_host_pad_down(bind))
               unreleased_pads.push_back(bind);
      }
}

bool HotkeyBindings::waiting(const HotkeyBinding& binding) const
{
   unreleased_keys.erase(std::remove_if(unreleased_keys.begin(), unreleased_keys.end(),
         [](unsigned code) { return !rib_host_key_down(code); }), unreleased_keys.end());
   unreleased_pads.erase(std::remove_if(unreleased_pads.begin(), unreleased_pads.end(),
         [](unsigned bind) { return !rib_host_pad_down(bind); }), unreleased_pads.end());
   if (is_key(binding))
      return std::find(unreleased_keys.begin(), unreleased_keys.end(), binding.code)
            != unreleased_keys.end();
   return std::any_of(binding.binds.begin(), binding.binds.end(), [this](unsigned bind) {
      return std::find(unreleased_pads.begin(), unreleased_pads.end(), bind) != unreleased_pads.end();
   });
}

bool HotkeyBindings::held(Hotkey hotkey, bool keys) const
{
   for (const HotkeyBinding& binding : lists[at(hotkey)])
   {
      if (!counts_for(hotkey, binding) || waiting(binding))
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

std::vector<unsigned> HotkeyBindings::keys(Hotkey hotkey) const
{
   std::vector<unsigned> codes;
   for (const HotkeyBinding& binding : lists[at(hotkey)])
      if (is_key(binding) && counts_for(hotkey, binding) && !waiting(binding))
         codes.push_back(binding.code);
   return codes;
}

bool HotkeyBindings::menu_key(unsigned code) const
{
   for (Hotkey hotkey : all)
      if (hotkey_acts(hotkey) != Acts::InGame)
         for (const HotkeyBinding& binding : lists[at(hotkey)])
            if (is_key(binding) && binding.code == code)
               return true;
   return false;
}

std::vector<unsigned> HotkeyBindings::menu_pad_binds() const
{
   std::vector<unsigned> binds;
   for (Hotkey hotkey : all)
      if (hotkey_acts(hotkey) != Acts::InGame)
         for (const HotkeyBinding& binding : lists[at(hotkey)])
         for (unsigned bind : binding.binds)
            if (std::find(binds.begin(), binds.end(), bind) == binds.end())
               binds.push_back(bind);
   return binds;
}
}
