#include "lists.hpp"
#include "document.hpp"
#include "elements.hpp"
#include <algorithm>
#include <unordered_map>

namespace rib {
void Lists::replace_rows(const char *list_id, const std::vector<Row>& rows)
{
   auto *list = list_element(list_id);
   auto *prototype = find_class(find_class(list, "list-prototype"), "list-row");
   auto *pager = find_class(list, "list-pager");
   if (!list || !prototype || !pager) return;
   const int size = std::max(1, list->GetAttribute<int>("data-page-size", 1));
   const int page_count = ((int)rows.size() + size - 1) / size;
   std::vector<Rml::Element*> pages;
   collect(list, "list-page", pages);
   std::unordered_map<std::string, Rml::Element*> existing;
   int current = 0;
   for (size_t index = 0; index < pages.size(); ++index) {
      if (!display_none(pages[index])) current = (int)index;
      std::vector<Rml::Element*> children;
      collect(pages[index], "list-row", children);
      for (auto *row : children) existing.emplace(row->GetId(), row);
   }
   current = std::max(0, std::min(current, page_count - 1));
   const std::string prefix = prototype->GetId();
   std::vector<const Row*> resized;
   for (int page_index = 0; page_index < page_count; ++page_index) {
      if (page_index == (int)pages.size()) {
         auto page = document.root()->CreateElement("div");
         page->SetClass("list-page", true);
         page->SetId(std::string(list_id) + "-page-" + std::to_string(page_index + 1));
         pages.push_back(list->InsertBefore(std::move(page), pager));
      }
      auto *page = pages[page_index];
      page->SetProperty("display", page_index == current ? "block" : "none");
      for (int index = page_index * size; index < std::min((page_index + 1) * size, (int)rows.size()); ++index) {
         const auto& data = rows[index];
         auto found = existing.find(data.id);
         auto *row = found == existing.end() ? nullptr : found->second;
         bool fit = !row;
         if (!row) {
            auto created = prototype->Clone();
            walk(created.get(), [&](Rml::Element *element) {
               const auto id = element->GetId();
               if (id.compare(0, prefix.size(), prefix) == 0)
                  element->SetId(data.id + id.substr(prefix.size()));
               return Walk::Continue;
            });
            row = page->InsertBefore(std::move(created), page->GetChild(index % size));
            wire_lists(row);
         } else {
            existing.erase(found);
            // Keep the nodes and listeners of unchanged rows. We move a row only
            // when the order changes, so focus and hover survive other updates.
            if (row != page->GetChild(index % size)) {
               auto moved = row->GetParentNode()->RemoveChild(row);
               page->InsertBefore(std::move(moved), page->GetChild(index % size));
            }
         }
         row->SetClass("selected", data.selected);
         const struct { const char *name; const std::string& text; } fields[] = {
            {"list-row-title", data.title}, {"list-row-detail", data.detail}, {"list-row-state", data.state}
         };
         for (const auto& field : fields) {
            if (auto *element = find_class(row, field.name)) {
               const auto escaped = Rml::StringUtilities::EncodeRml(field.text);
               if (element->GetInnerRML() != escaped) { element->SetInnerRML(escaped); fit = true; }
            }
         }
         auto *icon = find_class(row, "list-row-icon");
         if (!data.icon.empty()) {
            if (!icon) {
               auto created = document.root()->CreateElement("img");
               created->SetClass("list-row-icon", true);
               icon = row->AppendChild(std::move(created));
            }
            if (icon->GetAttribute<std::string>("src", "") != data.icon) icon->SetAttribute("src", data.icon);
            icon->RemoveProperty("display");
         } else if (icon) icon->SetProperty("display", "none");
         if (fit) resized.push_back(&data);
      }
   }
   for (const auto& item : existing) item.second->GetParentNode()->RemoveChild(item.second);
   for (size_t index = page_count; index < pages.size(); ++index) list->RemoveChild(pages[index]);
   pager->SetProperty("display", page_count > 1 ? "block" : "none");
   if (auto *count = find_class(pager, "list-pager-count"))
      count->SetInnerRML(std::to_string(current + 1) + "/" + std::to_string(page_count));
   mark_pager(list, current, page_count);
   if (!resized.empty()) document.get_context()->Update();
   for (const auto *row : resized) fit_row_title(row->id.c_str(), row->title.c_str());
}
}
