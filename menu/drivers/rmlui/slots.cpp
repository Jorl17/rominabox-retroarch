#include "words.hpp"
#include "document_contract.hpp"
#include "slots.hpp"

#include "document.hpp"
#include "elements.hpp"
#include "focus.hpp"
#include "status.hpp"
#include <RmlUi/Core/StringUtilities.h>
#include <encodings/crc32.h>
#include <streams/file_stream.h>
#include <algorithm>
#include <cstdlib>
#include <vector>

#ifndef RIB_RMLUI_HEADLESS
#include "../third_party/lodepng.h"
#endif

namespace rib {
Slots::Picture Slots::read_picture(const std::string& path)
{
   Picture picture;
   void *bytes = nullptr;
   int64_t size = 0;
   if (path.empty() || !filestream_read_file(path.c_str(), &bytes, &size))
      return picture;
   picture.bytes.assign(static_cast<unsigned char*>(bytes),
         static_cast<unsigned char*>(bytes) + size);
   free(bytes);
   picture.version = std::to_string(size) + ":" + std::to_string(
         encoding_crc32(0, picture.bytes.data(), picture.bytes.size()));
   return picture;
}

bool Slots::picture_ready(const std::string& path, const Picture& picture)
{
#ifndef RIB_RMLUI_HEADLESS
   /* Decode a picture written while the game runs before its texture goes
    * into the RmlUi cache. */
   (void)path;
   std::vector<unsigned char> pixels;
   unsigned width = 0, height = 0;
   return lodepng::decode(pixels, width, height, picture.bytes) == 0;
#else
   (void)picture;
   return !path.empty();
#endif
}

std::string Slots::quoted_css_path(const std::string& path)
{
   std::string result;
   for (char value : path)
   {
      if (value == '\\' || value == '"') result += '\\';
      result += value;
   }
   return result;
}

void Slots::paint() const
{
   if (!document.root()) return;

   /* We mark focus with `focused`, and a slot `selected` with or without it. */
   for (int index = 0; index < kSlotCount; ++index)
   {
      const int slot = index + 1;
      const std::string suffix = std::to_string(slot);
      if (auto *element = document.root()->GetElementById(document_contract::Slot + suffix))
      {
         element->SetClass(document_contract::Selected, slot == selected_slot);
         element->SetClass(document_contract::Occupied, slots[index].occupied);
         element->SetClass(document_contract::Empty, !slots[index].occupied);
      }
      document.set_element_text((document_contract::SlotLabelPrefix + suffix).c_str(),
            say(Word::Slot, {{"slot", suffix}}).c_str());
      document.set_element_text((document_contract::SlotStatePrefix + suffix).c_str(),
            say(slots[index].occupied ? Word::Occupied : Word::Empty).c_str());
      if (auto *image = document.root()->GetElementById(document_contract::SlotImagePrefix + suffix))
      {
         if (slots[index].occupied && !slots[index].thumbnail_path.empty())
            image->SetProperty("decorator", "image(\"" +
                  quoted_css_path(slots[index].thumbnail_path) + "\" fill)");
         else
            image->RemoveProperty("decorator");
      }
   }

   document.set_disabled(document_contract::Load, !slots[selected_slot - 1].occupied);
   document.show_fact(document_contract::ChosenSlotFact, std::to_string(selected_slot));
   paint_status_line(document.root()->GetElementById(document_contract::Status),
         status.main_text());
}

void Slots::set_selected_slot(int slot)
{
   if (!valid_slot(slot)) return;
   selected_slot = slot;
   paint();
}

void Slots::set_slot_state(int slot, bool occupied, const char *thumbnail_path)
{
   if (!valid_slot(slot)) return;
   SlotState& state = slots[slot - 1];
   const std::string next_path = thumbnail_path ? thumbnail_path : "";
   const Picture picture = read_picture(next_path);
   if (state.occupied == occupied && state.thumbnail_path == next_path &&
         state.thumbnail_version == picture.version)
      return;
   state.occupied = occupied;
   if (!next_path.empty() && !picture_ready(next_path, picture))
   {
      paint();
      return;
   }
   if (!state.thumbnail_path.empty())
      document.release_texture(state.thumbnail_path);
   state.thumbnail_path = next_path;
   state.thumbnail_version = picture.version;
   paint();
}

bool Slots::occupied(int slot) const
{
   return valid_slot(slot) && slots[slot - 1].occupied;
}

bool Slots::has_thumbnail(int slot) const
{
   return valid_slot(slot) && !slots[slot - 1].thumbnail_path.empty();
}
}
