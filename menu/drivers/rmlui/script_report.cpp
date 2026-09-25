#include "document_contract.hpp"
#include "script.hpp"

#ifdef RIB_MENU_SCRIPT
#include "view.hpp"
#include "elements.hpp"
#include <cmath>
#include <cstdio>
#if defined(RIB_ACHIEVEMENTS_TEST) && defined(HAVE_CHEEVOS)
#include "../../../cheevos/rominabox.h"
#endif

const char *rib::Script::report(const char *screen, bool menu_open,
      bool transfer_pending, bool capture_active, const char *profile,
      float volume_db)
{
   std::string& report = report_text;
   const auto quote = [](const std::string& value) {
      std::string out = "\"";
      for (unsigned char ch : value)
      {
         if (ch == '\\' || ch == '"') out += '\\';
         if (ch < 32)
         {
            char escaped[7];
            std::snprintf(escaped, sizeof(escaped), "\\u%04x", ch);
            out += escaped;
         }
         else out += ch;
      }
      return out + '"';
   };
   const auto boolean = [](bool value) { return value ? "true" : "false"; };
   report = "{\"screen\":" + quote(screen ? screen : "")
      + ",\"menuOpen\":" + boolean(menu_open)
      + ",\"transferPending\":" + boolean(transfer_pending)
      + ",\"captureActive\":" + boolean(capture_active)
      + ",\"profile\":" + quote(profile ? profile : "")
      + ",\"volumeDb\":" + std::to_string(volume_db);
   for (const char *name : {document_contract::Focused, document_contract::Selected, document_contract::Capturing, document_contract::Disabled,
         document_contract::On, document_contract::Showing, document_contract::Leaving})
   {
      report += "," + quote(name) + ":[";
      std::vector<Rml::Element*> found;
      collect(view.document.root(), name, found);
      bool comma = false;
      for (Rml::Element *element : found)
      {
         if (element->GetId().empty() || hidden(element)) continue;
         if (comma) report += ',';
         report += quote(element->GetId());
         comma = true;
      }
      report += ']';
   }
   report += ",\"text\":{";
   bool comma = false;
   for (const char *id : {document_contract::Heading, document_contract::FooterHint, document_contract::Status,
         document_contract::ControlsStatus, document_contract::ControlsDeviceCurrent, "volume-value", "shaders-page-count",
         "achievements-page-count", document_contract::AchievementsState, "control-binds"})
   {
      Rml::Element *element = view.document.root() ? view.document.root()->GetElementById(id) : nullptr;
      if (!element || hidden(element)) continue;
      if (comma) report += ',';
      report += quote(id) + ':' + quote(element->GetInnerRML());
      comma = true;
   }
   report += "},\"slots\":[";
   for (int index = 0; index < rib::kSlotCount; ++index)
   {
      if (index) report += ',';
      report += std::string("{\"occupied\":") + boolean(view.slots.occupied(index + 1))
         + ",\"thumbnail\":" + boolean(view.slots.has_thumbnail(index + 1)) + '}';
   }
   report += "],\"sliders\":{";
   comma = false;
   for (const auto& slider : view.parts.fractions())
   {
      if (comma) report += ',';
      report += quote(slider.first) + ':' + std::to_string(slider.second);
      comma = true;
   }
   report += "},\"bindsBox\":[";
   Rml::Element *binds = view.document.root() ? view.document.root()->GetElementById("control-binds") : nullptr;
   if (binds && !hidden(binds))
   {
      const auto at = binds->GetAbsoluteOffset(Rml::BoxArea::Border);
      const auto size = binds->GetBox().GetSize(Rml::BoxArea::Border);
      for (float value : {at.x, at.y, size.x, size.y})
      {
         if (report.back() != '[') report += ',';
         report += std::to_string(static_cast<int>(std::lround(value)));
      }
   }
   report += "]";
#if defined(RIB_ACHIEVEMENTS_TEST) && defined(HAVE_CHEEVOS)
   {
      rib_achievements_snapshot_t snapshot{};
      rib_achievements_get_snapshot(&snapshot);
      report += ",\"achievements\":{\"status\":" + std::to_string(snapshot.status)
         + ",\"revision\":" + std::to_string(snapshot.revision)
         + ",\"enabled\":" + boolean(snapshot.enabled_preference)
         + ",\"pendingUpload\":" + boolean(snapshot.pending_upload)
         + ",\"startupWaiting\":" + boolean(snapshot.startup_waiting)
         + ",\"startupSkipped\":" + boolean(snapshot.startup_skipped)
         + ",\"account\":" + quote(snapshot.account)
         + ",\"error\":" + quote(snapshot.error)
         + ",\"rows\":[";
      for (size_t index = 0; index < snapshot.count; ++index)
      {
         rib_achievement_row_t row{};
         if (!rib_achievements_get_row(index, &row)) continue;
         if (report.back() != '[') report += ',';
         report += "{\"id\":" + std::to_string(row.id)
            + ",\"state\":" + std::to_string(row.state)
            + ",\"title\":" + quote(row.title) + '}';
      }
      report += "]}";
   }
#endif
   report += '}';
   return report.c_str();
}
#endif
