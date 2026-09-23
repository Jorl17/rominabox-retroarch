#include "settings.hpp"
#include "host.h"
#include "files.h"
#include "document.hpp"
#include "lists.hpp"
#include "parts.hpp"
#include "slots.hpp"
#include "../../../audio/volume_range.h"
#include "../../../verbosity.h"
#include <streams/file_stream.h>
#include <string/stdstring.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace rib {
namespace {
/* Where we store the position of a switch, in the game's storage. */
static bool rib_toggle_path(const char *data, const char *id, char *out, size_t length)
{
   if (!data || !*data || !id || !*id)
      return false;
   snprintf(out, length, "%s/toggle-%s", data, id);
   return true;
}

static void rib_toggle_remember(const rib_toggle_t *toggle, const char *data)
{
   char path[PATH_MAX_LENGTH];
   const char *body = toggle->state ? "1\n" : "0\n";

   if (!rib_toggle_path(data, toggle->id, path, sizeof(path)))
      return;
   if (!filestream_write_file(path, body, (int64_t)strlen(body)))
      RARCH_ERR("[RIB] the switch '%s' is %s, but %s could not be written, so "
            "the next launch will start from the design's default.\n",
            toggle->id, toggle->state ? "on" : "off", path);
}

static bool rib_toggle_recall(rib_toggle_t *toggle, const char *data)
{
   char path[PATH_MAX_LENGTH];
   int64_t length = 0;
   char *body = NULL;

   if (!rib_toggle_path(data, toggle->id, path, sizeof(path)))
      return false;
   if (!filestream_read_file(path, (void**)&body, &length) || !body)
      return false;
   toggle->state = length > 0 && body[0] == '1';
   free(body);
   return true;
}

}

void Toggles::load(const rib_design_data& design, const char *data)
{
   count = design.toggle_count;
   for (int index = 0; index < count; ++index)
   {
      entries[index] = design.toggles[index];
      rib_toggle_recall(&entries[index], data);
   }
}

/* The combined effect of all switches. We combine them instead of applying
 * them in turn, so of two switches that lock the slots, the last does not win. */
void Toggles::apply() const
{
   const rib_toggle_t *guarding = NULL;
   int index;

   for (index = 0; index < count; ++index)
   {
      const rib_toggle_t *toggle = &entries[index];
      lists.set_toggle(toggle->id,
            toggle->state ? toggle->on : toggle->off, toggle->state);
      if (toggle->state && toggle->guard == RIB_TOGGLE_GUARD_SAVES && !guarding)
         guarding = toggle;
   }
   slots.guard_slots(guarding ? guarding->guard_label : NULL,
         guarding ? guarding->guard_status : NULL);
}

void Toggles::toggle(const char *id, const char *data)
{
   if (!id || !*id)
      return;
   for (int index = 0; index < count; ++index)
   {
      auto& entry = entries[index];
      if (!string_is_equal(entry.id, id))
         continue;
      entry.state = !entry.state;
      rib_toggle_remember(&entry, data);
      apply();
      break;
   }
}

void Volume::configure_path(const char *data)
{
   if (data && *data)
      snprintf(path, sizeof(path), "%s/%s", data, RIB_VOLUME_FILE);
}

void Volume::initialize()
{
   parts.set_slider_step(RIB_VOLUME_SLIDER_ID,
         AUDIO_VOLUME_STEP_DB / (AUDIO_VOLUME_MAX_DB - AUDIO_VOLUME_MIN_DB));
   // A stored level may be muted or above the maximum. Write it again only
   // when the quantized, unmuted level differs from the stored one.
   const float db = rib_host_volume();
   const bool muted = rib_host_muted();
   const float snapped = rib_volume_quantize_db(muted ? AUDIO_VOLUME_MIN_DB : db);
   set(snapped, muted || snapped != db);
}

void Volume::paint() const
{
   const float db = rib_volume_quantize_db(rib_host_volume());
   parts.set_slider(RIB_VOLUME_SLIDER_ID, rib_volume_fraction_from_db(db), "");
}

void Volume::set(float db, bool persist)
{
   db = rib_volume_quantize_db(db);
   rib_host_set_volume(db);
   if (persist && path[0])
      rib_write_menu_volume(path, db);
   paint();
}
}
