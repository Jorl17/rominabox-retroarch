#include "document_contract.hpp"
#include "lists.hpp"
#include "document.hpp"
#include "elements.hpp"
#include <algorithm>
#include <unordered_map>

namespace rib {
void Lists::replace_rows(const char *list_id, const std::vector<Row>& rows)
{
   auto *list = list_element(list_id);
   auto *prototype = find_class(find_class(list, document_contract::ListPrototype), document_contract::ListRow);
   auto *pager = find_class(list, document_contract::ListPager);
   if (!list || !prototype || !pager) return;
   const int size = std::max(1, list->GetAttribute<int>(document_contract::PageSizeAttribute, 1));
   const int page_count = ((int)rows.size() + size - 1) / size;
   std::vector<Rml::Element*> pages;
   collect(list, document_contract::ListPage, pages);
   std::unordered_map<std::string, Rml::Element*> existing;
   int current = 0;
   for (size_t index = 0; index < pages.size(); ++index) {
      if (!display_none(pages[index])) current = (int)index;
      std::vector<Rml::Element*> children;
      collect(pages[index], document_contract::ListRow, children);
      for (auto *row : children) existing.emplace(row->GetId(), row);
   }
   current = std::max(0, std::min(current, page_count - 1));
   const std::string prefix = prototype->GetId();
   std::vector<const Row*> resized;
   for (int page_index = 0; page_index < page_count; ++page_index) {
      if (page_index == (int)pages.size()) {
         auto page = document.root()->CreateElement("div");
         page->SetClass(document_contract::ListPage, true);
         page->SetId(std::string(list_id) + "-page-" + std::to_string(page_index + 1));
         pages.push_back(list->InsertBefore(std::move(page), pager));
      }
      auto *page = pages[page_index];
      show(page, page_index == current);
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
         row->SetClass(document_contract::Selected, data.selected);
         row->SetClass(document_contract::Line, data.line);
         const struct { const char *name; const std::string& text; } fields[] = {
            {document_contract::ListRowTitle, data.title}, {document_contract::ListRowDetail, data.detail}, {document_contract::ListRowState, data.state}
         };
         for (const auto& field : fields) {
            if (auto *element = find_class(row, field.name)) {
               const auto escaped = Rml::StringUtilities::EncodeRml(field.text);
               if (element->GetInnerRML() != escaped) { element->SetInnerRML(escaped); fit = true; }
            }
         }
         // For a badge that is loading, we show the placeholder in the row
         // prototype from the design. We make an image only from a picture
         // that has arrived, because in RmlUi an image without a picture is a
         // white box over the design.
         const bool loading = data.badge == Row::Badge::Loading;
         row->SetClass(document_contract::BadgeLoading, loading);
         auto *icon = find_class(row, document_contract::ListRowIcon);
         if (!data.icon.empty() && !loading) {
            if (!icon) {
               auto created = document.root()->CreateElement("img");
               created->SetClass(document_contract::ListRowIcon, true);
               icon = row->AppendChild(std::move(created));
            }
            if (icon->GetAttribute<std::string>("src", "") != data.icon) icon->SetAttribute("src", data.icon);
            show(icon, true);
         } else show(icon, false);
         if (fit) resized.push_back(&data);
      }
   }
   for (const auto& item : existing) item.second->GetParentNode()->RemoveChild(item.second);
   for (size_t index = page_count; index < pages.size(); ++index) list->RemoveChild(pages[index]);
   show(pager, page_count > 1);
   write_text(find_class(pager, document_contract::ListPagerCount),
         std::to_string(current + 1) + "/" + std::to_string(page_count));
   mark_pager(list, current, page_count);
   if (!resized.empty()) document.get_context()->Update();
   for (const auto *row : resized) fit_row_title(row->id.c_str(), row->title.c_str());
}
}
