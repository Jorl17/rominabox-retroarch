#include "words.hpp"
#include "document_contract.hpp"
#include "slots.hpp"

#include "document.hpp"
#include "elements.hpp"
#include "focus.hpp"
#include "status.hpp"
#include <RmlUi/Core/StringUtilities.h>
#include <sys/stat.h>
#include <algorithm>
#include <vector>

#ifndef RIB_RMLUI_HEADLESS
#include "../third_party/lodepng.h"
#endif

namespace rib {
std::string Slots::thumbnail_version(const std::string& path)
{
   struct stat info = {};
   if (path.empty() || stat(path.c_str(), &info) != 0) return {};
   long nanoseconds = 0;
#if defined(__APPLE__)
   nanoseconds = info.st_mtimespec.tv_nsec;
#elif !defined(_WIN32)
   nanoseconds = info.st_mtim.tv_nsec;
#endif
   return std::to_string(info.st_mtime) + ":" +
      std::to_string(nanoseconds) + ":" + std::to_string(info.st_size);
}

bool Slots::thumbnail_ready(const std::string& path)
{
#ifndef RIB_RMLUI_HEADLESS
   /* Decode async screenshots before their texture goes into the RmlUi cache. */
   std::vector<unsigned char> pixels;
   unsigned width = 0, height = 0;
   return lodepng::decode(pixels, width, height, path) == 0;
#else
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

   char row[16][64];
   const int row_count = focusable_ids(document.root(), document_contract::PausePanel, row, 16);
   for (int index = 0; index < row_count; ++index)
      if (auto *element = document.root()->GetElementById(row[index]))
         element->SetClass(document_contract::Focused, focus.pause_element() == row[index]);

   for (int index = 0; index < kSlotCount; ++index)
   {
      const int slot = index + 1;
      const std::string suffix = std::to_string(slot);
      if (auto *element = document.root()->GetElementById(document_contract::Slot + suffix))
      {
         element->SetClass(document_contract::Selected, slot == selected_slot);
         element->SetClass(document_contract::Focused, focus.highlighted_slot() == slot);
         element->SetClass(document_contract::Occupied, slots[index].occupied);
         element->SetClass(document_contract::Empty, !slots[index].occupied);
         element->SetClass(document_contract::Disabled, !guard.empty());
         if (guard.empty())
            element->RemoveAttribute("disabled");
         else
            element->SetAttribute("disabled", "disabled");
      }
      if (auto *label = document.root()->GetElementById(document_contract::SlotLabelPrefix + suffix))
         label->SetInnerRML(rib::words::SlotLabel + suffix);
      if (auto *state = document.root()->GetElementById(document_contract::SlotStatePrefix + suffix))
         state->SetInnerRML(guard.empty()
               ? (slots[index].occupied ? rib::words::Occupied : rib::words::Empty)
               : Rml::StringUtilities::EncodeRml(guard));
      if (auto *image = document.root()->GetElementById(document_contract::SlotImagePrefix + suffix))
      {
         const float height = std::min(138.0f, 230.0f / game_aspect);
         if (auto *picture = image->GetParentNode())
         {
            picture->SetProperty("width", std::to_string(height * game_aspect) + "dp");
            picture->SetProperty("height", std::to_string(height) + "dp");
            picture->SetProperty("margin-top", std::to_string((138.0f - height) / 2) + "dp");
            picture->SetProperty("margin-bottom", std::to_string((138.0f - height) / 2) + "dp");
         }
         if (slots[index].occupied && !slots[index].thumbnail_path.empty())
            image->SetProperty("decorator", "image(\"" +
                  quoted_css_path(slots[index].thumbnail_path) + "\" fill)");
         else
            image->RemoveProperty("decorator");
      }
   }

   for (const char *id : {document_contract::Save, document_contract::Load})
      if (auto *button = document.root()->GetElementById(id))
      {
         const bool disabled = !guard.empty() ||
               (std::string(id) == document_contract::Load && !slots[selected_slot - 1].occupied);
         button->SetClass(document_contract::Disabled, disabled);
         if (disabled)
            button->SetAttribute("disabled", "disabled");
         else
            button->RemoveAttribute("disabled");
      }

   if (auto *line = document.root()->GetElementById(document_contract::Status))
   {
      /* When there is nothing to report, we show the text the line had in the
       * design, which we keep from the document as loaded. */
      if (!line->HasAttribute("data-prompt"))
         line->SetAttribute("data-prompt", line->GetInnerRML());
      const std::string& shown = status.main_text().empty()
            ? guard_reason : status.main_text();
      line->SetInnerRML(shown.empty()
            ? line->GetAttribute<Rml::String>("data-prompt", "")
            : Rml::StringUtilities::EncodeRml(shown));
   }
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
   const std::string version = thumbnail_version(next_path);
   if (state.occupied == occupied && state.thumbnail_path == next_path &&
         state.thumbnail_version == version)
      return;
   state.occupied = occupied;
   if (!next_path.empty() && !thumbnail_ready(next_path))
   {
      paint();
      return;
   }
   if (!state.thumbnail_path.empty())
      document.release_texture(state.thumbnail_path);
   state.thumbnail_path = next_path;
   state.thumbnail_version = version;
   paint();
}

void Slots::set_game_aspect(float aspect)
{
   if (!(aspect > 0.0f && aspect < 100.0f) || aspect == game_aspect)
      return;
   game_aspect = aspect;
   paint();
}

void Slots::guard_slots(const char *label, const char *reason)
{
   const std::string next = label ? label : "";
   const std::string why = reason ? reason : "";
   if (next == guard && why == guard_reason) return;
   guard = next;
   guard_reason = why;
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

namespace rib {
void Slots::focus_action(const Event& event)
{
   focus.pause_action(event);
   paint();
}
void Slots::focus_element(const char *id)
{
   focus.pause_element(id);
   paint();
}
}
