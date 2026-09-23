#include "document_contract.hpp"
#include "status.hpp"

#include "document.hpp"
#include <RmlUi/Core/StringUtilities.h>

namespace rib {
void Status::show(Message& message, const char *id, const char *text)
{
   message.text = text ? text : "";
   message.expires = document.elapsed() + 5.0;
   if (document.root())
      if (auto *element = document.root()->GetElementById(id))
         element->SetInnerRML(Rml::StringUtilities::EncodeRml(message.text));
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

void Status::expire()
{
   expire(main, document_contract::Status);
   expire(controls, document_contract::ControlsStatus);
}
}
