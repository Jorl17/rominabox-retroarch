#include "document_contract.hpp"
#include "lists.hpp"
#include "document.hpp"
#include "elements.hpp"
#include "paging.hpp"
#include <unordered_map>

namespace rib {
void Lists::replace_rows(const char *list_id, const std::vector<Row>& rows)
{
   auto *list = list_element(list_id);
   auto *prototype = find_class(find_class(list, document_contract::ListPrototype), document_contract::ListRow);
   auto *pager = find_class(list, document_contract::ListPager);
   if (!list || !prototype || !pager) return;
   std::vector<Rml::Element*> pages;
   collect(list, document_contract::ListPage, pages);
   const int current = paging::current(list);
   std::unordered_map<std::string, Rml::Element*> existing;
   for (auto *row : paging::rows(list)) existing.emplace(row->GetId(), row);
   const std::string prefix = prototype->GetId();
   std::vector<Rml::Element*> ordered;
   std::vector<const Row*> resized;
   for (const auto& data : rows) {
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
         if (pages.empty()) pages.push_back(paging::add_page(list));
         // We place it on its page when we split the list, with the other rows.
         row = pages.back()->AppendChild(std::move(created));
         wire_lists(row);
      } else
         existing.erase(found);
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
         const std::string source = to_rml_path(data.icon);
         if (icon->GetAttribute<std::string>("src", "") != source) icon->SetAttribute("src", source);
         show(icon, true);
      } else show(icon, false);
      ordered.push_back(row);
      if (fit) resized.push_back(&data);
   }
   for (const auto& item : existing) item.second->GetParentNode()->RemoveChild(item.second);
   paging::split(list, ordered, current);
   if (!resized.empty()) document.get_context()->Update();
   for (const auto *row : resized) fit_row_title(row->id.c_str(), row->title.c_str());
}
}
