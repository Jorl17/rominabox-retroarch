/* The session facts, which we read from the environment set by the launcher,
 * in the one way we read it in the player (rominabox_environment.h). The
 * environment does not change during a run, so we read each fact once. */
#include "rominabox_session.h"

#include <stdlib.h>
#include <string.h>

#include "rominabox_environment.h"

static bool rib_is_set(const char *name)
{
   char *value = rib_environment(name);
   bool set    = value != NULL;
   free(value);
   return set;
}

static bool rib_is_one(const char *name)
{
   char *value = rib_environment(name);
   bool one    = value && strcmp(value, "1") == 0;
   free(value);
   return one;
}

const char *rib_session_title(void)
{
   static bool read;
   static char *title;
   if (!read)
   {
      read  = true;
      title = rib_environment(RIB_ENV_TITLE);
      if (title && !title[0])
      {
         free(title);
         title = NULL;
      }
   }
   return title;
}

bool rib_session_advanced_access(void)
{
   return rib_is_one(RIB_ENV_ADVANCED_ACCESS);
}

bool rib_session_restricted(void)
{
   return rib_session_title() && !rib_session_advanced_access();
}

bool rib_session_window_shown(void)
{
   return rib_is_one(RIB_ENV_SHOW_WINDOW);
}

bool rib_session_window_hidden(void)
{
   return rib_is_set(RIB_ENV_QUIET) && !rib_session_window_shown();
}

bool rib_session_menu_shot(void)
{
   return rib_is_set(RIB_ENV_MENU_SHOT);
}
