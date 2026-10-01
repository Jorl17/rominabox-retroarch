#include "script.hpp"

#ifdef RIB_MENU_SCRIPT
#include "menu_api.h"
#include "host.h"
#include "view.hpp"
#include "elements.hpp"
#include <cmath>
#include "../../../verbosity.h"
#include "../../../rominabox_environment.h"
#include "../../../rominabox_launch.h"
#include <string/stdstring.h>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>

void rib::Script::shot()
{
   const rib_environment_value shot_path = rib_owned(rib_environment(RIB_ENV_MENU_SHOT));
   const char *path = shot_path.get();

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
   view.document.capture_next(path);
   rib_host_end_after_script_shot(path);
}

void rib::Script::run(void *menu, const ScriptObservation& observation)
{
   const char *comma;
   size_t length;

   if (!started)
   {
      const rib_environment_value given = rib_owned(rib_environment(RIB_ENV_MENU_SCRIPT));
      started = true;
      running = scripted = given != nullptr;
      {
         const rib_environment_value at_menu = rib_owned(rib_environment(RIB_ENV_START_AT_MENU));
         awaits_menu = at_menu && string_is_equal(at_menu.get(), "1");
      }
      if (given)
      {
         steps = given.get();
         RARCH_LOG("[RIB] menu script: %s\n", steps.empty() ? "(none)" : steps.c_str());
      }
   }
   if (!scripted)
      return;
   if (awaits_menu)
   {
      if (!rib_host_menu_open())
         return;
      awaits_menu = false;
   }
   const char *script = steps.c_str();

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
         shot();
      }
      return;
   }

   comma  = strchr(script + at, ',');
   length = comma ? (size_t)(comma - (script + at)) : strlen(script + at);
   const std::string command(script + at, length);
   const char *id = command.c_str();
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

   /* Press and release a key, as we read keys in the menu. We read the hotkeys
    * for use during play from these keys. */
   if (!strncmp(id, "press:", 6))
   {
      if (rib_host_script_press(id + 6))
         RARCH_LOG("[RIB] menu script pressed %s.\n", id + 6);
      else
      {
         RARCH_ERR("[RIB] menu script names no key '%s'; stopping.\n", id + 6);
         rib_host_quit();
      }
      return;
   }

   if (!strncmp(id, "report:", 7))
   {
      binds_list = observation.binds_list ? observation.binds_list : "";
      float volume_db = 0.0f;
      rib_host_setting(RIB_SETTING_AudioVolume, &volume_db);
      if (menu)
         fprintf(stderr, "[RIB] checkpoint %s %s\n", id + 7,
               report(observation.screen, rib_host_menu_open(),
                     observation.transfer_pending, observation.capture_active, observation.profile,
                     volume_db));
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

   /* The Alt+Enter command. We restart the video driver for fullscreen or for
    * a window. */
   if (!strcmp(id, "fullscreen"))
   {
      rib_host_toggle_fullscreen();
      RARCH_LOG("[RIB] menu script toggled fullscreen.\n");
      return;
   }

   if (!strncmp(id, "hover:", 6))
   {
      hover = id + 6;
      if (!view.move_pointer_to(hover.c_str()))
      {
         RARCH_ERR("[RIB] menu script cannot hover '%s'; stopping so no "
               "screenshot is taken of the wrong screen.\n", hover.c_str());
         rib_host_quit();
      }
      return;
   }

   {
      const size_t mark = command.find('@');
      if (mark != std::string::npos)
      {
         const std::string slider = command.substr(0, mark);
         if (!view.parts.commit_slider(slider.c_str(), (float)strtof(id + mark + 1, NULL)))
         {
            RARCH_ERR("[RIB] menu script names no slider '%s'; stopping so no "
                  "screenshot is taken of the wrong screen.\n", slider.c_str());
            rib_host_quit();
         }
         return;
      }
   }

   if (!view.document.click_element(id))
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
   if (running && !hover.empty())
      view.move_pointer_to(hover.c_str());
}
#endif
