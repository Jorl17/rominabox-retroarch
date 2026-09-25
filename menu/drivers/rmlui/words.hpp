#pragma once

#include <cstddef>
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

namespace rib {
/* The words we write in the menu ourselves, as declared in words.inc. */
enum class Word
{
#define RIB_WORD(name, id, english) name,
#include "words.inc"
};

inline constexpr size_t kWordCount = 0
#define RIB_WORD(name, id, english) + 1
#include "words.inc"
      ;

/* The id for `word` in design.json and design.cfg. */
const char *word_id(Word word);
/* The word with the id `id`, or false when there is no word with that id. */
bool word_named(const std::string& id, Word& word);

/* The one lookup for every word in the menu: the wording in the loaded
 * design, or else the English. */
const std::string& say(Word word);
/* `word` with each {name} in it replaced by its value. */
std::string say(Word word, std::initializer_list<std::pair<const char *, std::string>> values);

/* Put the words from the design over the English, word by word, and use
 * the English for the rest. Once each time we load a design. */
void use_words(const std::vector<std::pair<Word, std::string>>& given);
}
