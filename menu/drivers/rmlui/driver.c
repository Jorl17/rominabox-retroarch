#include "../../menu_driver.h"
#include "../rmlui_bridge.h"
#include "menu_api.h"
#include <stdlib.h>

typedef struct rib_driver { void *menu; } rib_driver;

static void *driver_init(void **userdata, bool threaded)
{
   menu_handle_t *handle = (menu_handle_t*)calloc(1, sizeof(*handle));
   rib_driver *driver = (rib_driver*)calloc(1, sizeof(*driver));
   (void)threaded;
   if (!handle || !driver || !(driver->menu = rib_menu_create()))
   {
      free(handle);
      free(driver);
      return NULL;
   }
   *userdata = driver;
   return handle;
}

static void driver_free(void *data)
{
   rib_driver *driver = (rib_driver*)data;
   if (driver)
   {
      rib_menu_destroy(driver->menu);
      driver->menu = NULL;
   }
   /* The call that frees data is in RetroArch, after this cleanup function. */
}

static void driver_frame(void *data, video_frame_info_t *video)
{
   rib_driver *driver = (rib_driver*)data;
   if (driver && video)
      rib_menu_frame(driver->menu, (int)VIDEO_SCALE_W(video->dims), (int)VIDEO_SCALE_H(video->dims));
}
/* The RetroArch loop, between frames. We do here what the player asked for,
 * outside the frame of the video driver. */
static void driver_render(void *data, unsigned dims, bool is_idle)
{
   rib_driver *driver = (rib_driver*)data;
   (void)dims;
   (void)is_idle;
   if (driver)
      rib_menu_update(driver->menu);
}
static void driver_reset(void *data, bool threaded)
{
   (void)threaded;
   if (data) rib_menu_context_reset(((rib_driver*)data)->menu);
}
static void driver_destroy_context(void *data)
{
   if (data) rib_menu_context_destroy(((rib_driver*)data)->menu);
}
static void driver_toggle(void *data, bool on)
{
   if (data) rib_menu_toggle(((rib_driver*)data)->menu, on);
}
bool rib_rmlui_consume_menu_toggle(void *data)
{
   return data && rib_menu_consume_toggle(((rib_driver*)data)->menu);
}

static int driver_action(void *data, menu_entry_t *entry, size_t index, enum menu_action action)
{
   enum rib_key key = RIB_KEY_NOOP;
   (void)entry;
   (void)index;
   switch (action)
   {
      case MENU_ACTION_UP: key = RIB_KEY_UP; break;
      case MENU_ACTION_DOWN: key = RIB_KEY_DOWN; break;
      case MENU_ACTION_LEFT: key = RIB_KEY_LEFT; break;
      case MENU_ACTION_RIGHT: key = RIB_KEY_RIGHT; break;
      case MENU_ACTION_OK: key = RIB_KEY_OK; break;
      case MENU_ACTION_CANCEL: key = RIB_KEY_CANCEL; break;
      case MENU_ACTION_SELECT: key = RIB_KEY_SELECT; break;
      case MENU_ACTION_START: key = RIB_KEY_START; break;
      case MENU_ACTION_TOGGLE: key = RIB_KEY_TOGGLE; break;
      case MENU_ACTION_RESUME: key = RIB_KEY_RESUME; break;
      default: break;
   }
   return data ? rib_menu_key(((rib_driver*)data)->menu, key) : 0;
}

static int driver_bind(menu_file_list_cbs_t *cbs, const char *path,
      const char *label, unsigned type, size_t index)
{
   (void)cbs; (void)path; (void)label; (void)type; (void)index;
   return 0;
}

menu_ctx_driver_t menu_ctx_rmlui = {
   .frame = driver_frame,
   .render = driver_render,
   .init = driver_init,
   .free = driver_free,
   .context_reset = driver_reset,
   .context_destroy = driver_destroy_context,
   .bind_init = driver_bind,
   .ident = "rmlui",
   .toggle = driver_toggle,
   .entry_action = driver_action
};
