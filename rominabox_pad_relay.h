/* ROM-in-a-Box: the controllers of a Windows game, read outside its sandbox.
 *
 * From inside its sandbox, a game cannot see the controllers in DirectInput,
 * so we read them for the game in the launcher, which runs outside it: once
 * per frame, on request from the game, at the same points as in RetroArch.
 * Between the two there is one block of memory mapped by both, and two
 * events. In the launcher we make them inheritable and put their handles in
 * RIB_ENV_PAD_RELAY (rominabox_launch.h). We write each controller as
 * DirectInput described it to the launcher, and its state as DirectInput
 * read it. In the player we pass them through our DirectInput stand-in
 * (input/drivers/rominabox_dinput.c) to the RetroArch joypad driver unchanged,
 * and we pass on the requests from the driver for a controller.
 *
 * From the game we write the request in `ask`, set the request event and wait
 * for the reply event, and in the launcher we do it between the two. Windows
 * only. We also include this in the launcher, built without RetroArch.
 *
 * The game also asks the launcher, through the same block, to export or
 * import the game's data (menu/drivers/rmlui/game_data_host.h), because only
 * outside the sandbox can it show a file dialog and reach the file the player
 * chooses. */
#ifndef RIB_PAD_RELAY_H
#define RIB_PAD_RELAY_H

#ifndef DIRECTINPUT_VERSION
#define DIRECTINPUT_VERSION 0x0800
#endif
#include <windows.h>
#include <dinput.h>

#include "rominabox_game_data.h"

/* As many controllers as RetroArch has users (MAX_USERS). */
#define RIB_PAD_RELAY_PADS 16
/* Every absolute axis a DIJOYSTATE2 reports. */
#define RIB_PAD_RELAY_AXES 8
/* The effects a controller may have at once. In RetroArch there is one for
 * each of its two motors. */
#define RIB_PAD_RELAY_EFFECTS 2
/* The axes an effect may move along. */
#define RIB_PAD_RELAY_EFFECT_AXES 2

typedef enum {
   /* List the controllers attached now, each set up again, as in the joypad
    * driver when it starts and whenever a device is added or removed. We
    * release what the game made for the previous list. */
   RIB_PAD_RELAY_LIST = 1,
   /* Read every controller. */
   RIB_PAD_RELAY_READ,
   /* Set an axis's range: DIPROP_RANGE by the axis's id. */
   RIB_PAD_RELAY_SET_RANGE,
   /* Make an effect, a constant force, in a free slot. */
   RIB_PAD_RELAY_MAKE_EFFECT,
   /* Change an effect's parameters, as listed in `flags` (DIEP_*). */
   RIB_PAD_RELAY_SET_EFFECT,
   RIB_PAD_RELAY_STOP_EFFECT,
   /* Let an effect go, and free its slot. */
   RIB_PAD_RELAY_DROP_EFFECT,
   /* The game's data: ask where to save the zip and write it there, ask for
    * a zip to import and check it, or set the chosen zip aside for the next
    * start and start the game again once it closes. */
   RIB_PAD_RELAY_EXPORT_DATA,
   RIB_PAD_RELAY_CHOOSE_IMPORT,
   RIB_PAD_RELAY_CONFIRM_IMPORT
} rib_pad_relay_asking;

/* What the launcher did with a request about the game's data: the answer
 * (rib_data_answer), the title of the game a chosen zip's data is from, and
 * why it failed, in UTF-8. */
typedef struct {
   LONG answer;
   char title[256];
   char sentence[1024];
} rib_pad_relay_data;

/* A constant-force effect, as in DIEFFECT but without its pointers. We
 * read only the parameters listed in the flags of the request. */
typedef struct {
   DWORD flags;
   DWORD duration;
   DWORD sample_period;
   DWORD gain;
   DWORD trigger_button;
   DWORD trigger_repeat_interval;
   DWORD start_delay;
   DWORD axis_count;
   DWORD axes[RIB_PAD_RELAY_EFFECT_AXES];
   LONG directions[RIB_PAD_RELAY_EFFECT_AXES];
   BOOL has_envelope;
   DIENVELOPE envelope;
   DICONSTANTFORCE force;
} rib_pad_relay_effect;

typedef struct {
   rib_pad_relay_asking what;
   DWORD pad;
   /* The axis for a range; the slot for an effect. */
   DWORD item;
   /* For SET_EFFECT: which parameters (DIEP_*). */
   DWORD flags;
   LONG range_min;
   LONG range_max;
   rib_pad_relay_effect effect;
   /* We write this in the launcher: what DirectInput returned and, for
    * MAKE_EFFECT, the slot in `item`. */
   HRESULT answer;
} rib_pad_relay_ask;

typedef struct {
   /* We write this in the launcher at each listing. */
   DIDEVICEINSTANCEA device;
   DWORD axis_count;
   DIDEVICEOBJECTINSTANCEA axes[RIB_PAD_RELAY_AXES];
   /* We write this in the launcher at each read: what GetDeviceState
    * returned, and the state from it. */
   HRESULT result;
   DIJOYSTATE2 state;
} rib_pad_relay_pad;

typedef struct {
   /* We write this in the launcher at each listing. */
   DWORD pad_count;
   rib_pad_relay_pad pads[RIB_PAD_RELAY_PADS];
   rib_pad_relay_ask ask;
   rib_pad_relay_data data;
} rib_pad_relay;

#endif
