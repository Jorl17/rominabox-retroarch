#include "pad_names.hpp"

#include "declarations.h"
#include "files.h"
#include "host.h"
#include "hotkey_bindings.hpp"
#include "words.hpp"
#include <file/config_file.h>
#include <cctype>
#include <cstring>

namespace rib {
namespace {
#define RIB_HOTKEY_PAD_CHORD(separator) const std::string chord = separator;
#include "hotkeys.inc"
}

void PadNames::load(const char *assets)
{
   standard.clear();
   const std::string defaults = std::string(assets ? assets : "") + "/" + files::HotkeysDefaults;
   config_file_t *config = config_file_new_from_path_to_string(defaults.c_str());
   if (!config)
      return;
   const std::string word = keys::PadWord("");
   struct config_file_entry entry;
   for (bool present = config_get_entry_list_head(config, &entry); present;
         present = config_get_entry_list_next(&entry))
      if (entry.key && entry.value && !std::strncmp(entry.key, word.c_str(), word.size()))
         standard[entry.key + word.size()] = entry.value;
   config_file_free(config);
}

std::string PadNames::position(const std::string& id) const
{
   unsigned bind = 0;
   char name[64];
   if (rib_host_pad_input(id.c_str(), &bind) && rib_host_pad_name(bind, name, sizeof(name)))
      return name;
   const auto found = standard.find(id);
   return found != standard.end() ? found->second : id;
}

std::string PadNames::input(const std::string& value) const
{
   unsigned bind = 0;
   if (rib_host_pad_value_input(value.c_str(), &bind))
      if (const char *id = rib_host_pad_input_id(bind))
         return position(id);
   if (!value.empty() && (value[0] == '+' || value[0] == '-'))
      return say(Word::PadAxis, {{"axis", value}});
   if (!value.empty() && value[0] == 'h')
   {
      size_t at = 1;
      while (at < value.size() && std::isdigit((unsigned char)value[at]))
         ++at;
      const std::string direction = value.substr(at);
      const Word word = direction == "up" ? Word::PadHatUp
            : direction == "down" ? Word::PadHatDown
            : direction == "left" ? Word::PadHatLeft
            : Word::PadHatRight;
      return say(word, {{"number", value.substr(1, at - 1)}});
   }
   return say(Word::PadButton, {{"number", value}});
}

std::string PadNames::binding(const HotkeyBinding& binding) const
{
   if (binding.kind == HotkeyBinding::Kind::Key)
      return key_word(binding.key.c_str());
   std::string text;
   for (size_t index = 0; index < binding.pads.size(); ++index)
   {
      const PadInput& pad = binding.pads[index];
      text += (index ? chord : std::string()) + (pad.bind ? position(pad.id) : input(pad.id));
   }
   return text;
}
}
