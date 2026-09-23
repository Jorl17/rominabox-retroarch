#pragma once

#include <RmlUi/Core.h>

namespace rib
{
/* Size the existing list from its text, then place its painted bounds clear of
 * the control labels, actions and screen edges, using a fixed candidate
 * ordering and margins. */
void place_binds_popup(Rml::ElementDocument *document, Rml::Context *context,
      const char *list_id, const char *anchor_id, int width_dp);
int popup_covered_labels(Rml::ElementDocument *document, Rml::Context *context,
      const char *anchor_id, int left, int top, int width, int height);
}
