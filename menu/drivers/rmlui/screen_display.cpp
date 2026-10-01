#include "screen_display.hpp"

#include "document_contract.hpp"
#include "elements.hpp"

namespace rib {
void display_screen(Rml::Element *root, const std::vector<ScreenDeclaration>& screens,
      const ScreenDeclaration& wanted)
{
   if (!root)
      return;
   for (const ScreenDeclaration& screen : screens)
      show(root->GetElementById(screen.panel), &screen == &wanted);
   write_text(root->GetElementById(document_contract::Heading), wanted.heading);
   if (!wanted.footer.empty())
      write_hint(root->GetElementById(document_contract::FooterHint), wanted.footer);
}
}
