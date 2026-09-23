#include "script.hpp"
#include "menu_api.h"
#include "host.h"
#include "../rmlui_bridge.h"
#include "../../../verbosity.h"
#include <string/stdstring.h>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static void rib_rmlui_script_shot(void)
{
   const char *path       = getenv("ROMINABOX_MENU_SHOT");

   if (!path || !*path || !rib_host_prepare_script_shot())
   {
      /* Without a screenshot, a script only drives the menu, so we leave the
       * game running and do not quit while someone may be playing it. */
      return;
   }

   /* We read the screenshot from the viewport and not from the framebuffer of
    * the core, because we draw the menu over the game and the framebuffer
    * contains only the game. We change the setting here, so there is no need
    * for a config override in the harness to get a picture of the menu. */

   /* We take the picture in the menu renderer, because the pixels are there:
    * the frame of the core with the menu drawn over it, still in the back
    * buffer. In RetroArch the screenshot code is in the runloop, after the
    * buffer is presented, and a viewport read at that point returns an empty
    * buffer, so the result is a black picture written without any error.
    *
    * We then end the run in the usual way, so no window stays open. */
   rib_rmlui_capture_next(path);
   rib_host_end_after_script_shot(path);
}

void rib::Script::run(void *menu, const ScriptObservation& observation)
{
   char id[128];
   const char *comma;
   size_t length;

   if (!started)
   {
      script  = getenv("ROMINABOX_MENU_SCRIPT");
      started = true;
      running = script != NULL;
      if (script)
         RARCH_LOG("[RIB] menu script: %s\n", *script ? script : "(none)");
   }
   if (!script)
      return;

   if (wait_until)
   {
      if (rib_host_time_us() < wait_until)
         return;
      wait_until = 0;
   }

   if (waiting > 0)
   {
      --waiting;
      return;
   }

   if (at >= strlen(script))
   {
      /* We have made every click. Wait for the menu to settle, take the
       * screenshot and let RetroArch exit by itself, so no window stays open. */
      if (settle-- <= 0)
      {
         settle = INT_MAX;
         running = false;
         /* After the clicks, including a disc change after the frames in which
          * the tray closes. For a row whose action never ran, we report the
          * index in the core, which is the disc it started on. */
         rib_host_script_finished();
         rib_rmlui_script_shot();
      }
      return;
   }

   comma  = strchr(script + at, ',');
   length = comma ? (size_t)(comma - (script + at)) : strlen(script + at);
   if (length >= sizeof(id))
      length = sizeof(id) - 1;
   memcpy(id, script + at, length);
   id[length] = '\0';
   at += length + (comma ? 1 : 0);

   if (!strncmp(id, "wait:", 5))
   {
      waiting = atoi(id + 5);
      RARCH_LOG("[RIB] menu script waiting %d frames.\n", waiting);
      return;
   }

   if (!strncmp(id, "wait-ms:", 8))
   {
      wait_until = rib_host_time_us()
            + (int64_t)atoi(id + 8) * 1000;
      RARCH_LOG("[RIB] menu script waiting %s ms.\n", id + 8);
      return;
   }

   if (!strncmp(id, "key:", 4))
   {
      static const struct { const char *name; enum rib_key action; } keys[] = {
         {"up", RIB_KEY_UP}, {"down", RIB_KEY_DOWN},
         {"left", RIB_KEY_LEFT}, {"right", RIB_KEY_RIGHT},
         {"ok", RIB_KEY_OK}, {"cancel", RIB_KEY_CANCEL},
         {"start", RIB_KEY_START}
      };
      unsigned key;
      for (key = 0; key < sizeof(keys) / sizeof(keys[0]); ++key)
         if (string_is_equal(id + 4, keys[key].name))
         {
            rib_menu_key(menu, keys[key].action);
            return;
         }
      RARCH_ERR("[RIB] menu script names no key '%s'; stopping.\n", id + 4);
      rib_host_quit();
      return;
   }

   if (!strncmp(id, "report:", 7))
   {
      if (menu)
         fprintf(stderr, "[RIB] checkpoint %s %s\n", id + 7,
               rib_rmlui_script_report(observation.screen, rib_host_menu_open(),
                     observation.transfer_pending, observation.capture_active, observation.profile,
                     rib_host_volume()));
      return;
   }

   /* The command for Escape, not a click. When the menu is closed there is no
    * element to click, so this is the only way to script pause and resume. */
   if (!strcmp(id, "toggle"))
   {
      rib_host_resume();
      RARCH_LOG("[RIB] menu script toggled the menu.\n");
      return;
   }

   if (!strncmp(id, "hover:", 6))
   {
      strlcpy(hover, id + 6, sizeof(hover));
      if (!rib_rmlui_move_pointer_to(hover))
      {
         RARCH_ERR("[RIB] menu script cannot hover '%s'; stopping so no "
               "screenshot is taken of the wrong screen.\n", hover);
         rib_host_quit();
      }
      return;
   }

   {
      char *mark = strchr(id, '@');
      if (mark)
      {
         *mark = '\0';
         if (!rib_rmlui_commit_slider(id, (float)strtof(mark + 1, NULL)))
         {
            RARCH_ERR("[RIB] menu script names no slider '%s'; stopping so no "
                  "screenshot is taken of the wrong screen.\n", id);
            rib_host_quit();
         }
         return;
      }
   }

   if (!rib_rmlui_click_element(id))
   {
      RARCH_ERR("[RIB] menu script names no element '%s'; stopping so no "
            "screenshot is taken of the wrong screen.\n", id);
      rib_host_quit();
      return;
   }
   RARCH_LOG("[RIB] menu script clicked '%s'.\n", id);
}

void rib::Script::restore_hover() const
{
   if (running && hover[0])
      rib_rmlui_move_pointer_to(hover);
}
