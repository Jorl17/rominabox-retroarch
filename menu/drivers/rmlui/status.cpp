#include "document_contract.hpp"
#include "status.hpp"

#include "document.hpp"
#include <RmlUi/Core/StringUtilities.h>

namespace rib {
void paint_status_line(Rml::Element *line, const std::string& text)
{
   if (!line) return;
   if (!line->HasAttribute(document_contract::PromptAttribute))
      line->SetAttribute(document_contract::PromptAttribute, line->GetInnerRML());
   const Rml::String shown = text.empty()
         ? line->GetAttribute<Rml::String>(document_contract::PromptAttribute, "")
         : Rml::StringUtilities::EncodeRml(text);
   if (line->GetInnerRML() != shown)
      line->SetInnerRML(shown);
}

void Status::show(Message& message, const char *id, const char *text)
{
   message.text = text ? text : "";
   message.expires = document.elapsed() + 5.0;
   if (document.root())
      paint_status_line(document.root()->GetElementById(id), message.text);
}

void Status::expire(Message& message, const char *id)
{
   if (!message.text.empty() && document.elapsed() >= message.expires)
      show(message, id, "");
}

void Status::set_main(const char *text)
{
   show(main, document_contract::Status, text);
}

void Status::set_controls(const char *text)
{
   show(controls, document_contract::ControlsStatus, text);
}

void Status::set_hotkeys(const char *text)
{
   show(hotkeys, document_contract::HotkeysStatus, text);
}

void Status::expire()
{
   expire(main, document_contract::Status);
   expire(controls, document_contract::ControlsStatus);
   expire(hotkeys, document_contract::HotkeysStatus);
}
}
