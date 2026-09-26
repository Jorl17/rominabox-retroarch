#include "words.hpp"

#include <cstring>

namespace rib {
namespace {
struct Known
{
   const char *id;
   const char *english;
};

const Known known[] = {
#define RIB_WORD(name, id, english) {id, english},
#include "words.inc"
};

constexpr size_t count = kWordCount;

const Known key_words[] = {
#define RIB_KEY_WORD(name, word) {name, word},
#include "key_words.inc"
};

std::vector<std::string>& wording()
{
   static std::vector<std::string> words = [] {
      std::vector<std::string> english;
      for (const Known& word : known)
         english.emplace_back(word.english);
      return english;
   }();
   return words;
}
}

const char *word_id(Word word)
{
   return known[static_cast<size_t>(word)].id;
}

bool word_named(const std::string& id, Word& word)
{
   for (size_t index = 0; index < count; ++index)
      if (id == known[index].id)
      {
         word = static_cast<Word>(index);
         return true;
      }
   return false;
}

const std::string& say(Word word)
{
   return wording()[static_cast<size_t>(word)];
}

std::string say(Word word, std::initializer_list<std::pair<const char *, std::string>> values)
{
   std::string text = say(word);
   for (const auto& value : values)
   {
      const std::string hole = std::string("{") + value.first + "}";
      for (size_t at = text.find(hole); at != std::string::npos;
            at = text.find(hole, at + value.second.size()))
         text.replace(at, hole.size(), value.second);
   }
   return text;
}

void use_words(const std::vector<std::pair<Word, std::string>>& given)
{
   std::vector<std::string>& words = wording();
   for (size_t index = 0; index < count; ++index)
      words[index] = known[index].english;
   for (const auto& word : given)
      words[static_cast<size_t>(word.first)] = word.second;
}

std::string key_word(const char *name)
{
   for (const Known& key : key_words)
      if (std::strcmp(name, key.id) == 0)
         return key.english;
   return name;
}
}
