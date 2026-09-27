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
 * only. We also include this in the launcher, built without RetroArch. */
#ifndef RIB_PAD_RELAY_H
#define RIB_PAD_RELAY_H

#ifndef DIRECTINPUT_VERSION
#define DIRECTINPUT_VERSION 0x0800
#endif
#include <windows.h>
#include <dinput.h>

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
   /* Read every controller. */
   RIB_PAD_RELAY_READ = 1,
   /* Set an axis's range: DIPROP_RANGE by the axis's id. */
   RIB_PAD_RELAY_SET_RANGE,
   /* Make an effect, a constant force, in a free slot. */
   RIB_PAD_RELAY_MAKE_EFFECT,
   /* Change an effect's parameters, as listed in `flags` (DIEP_*). */
   RIB_PAD_RELAY_SET_EFFECT,
   RIB_PAD_RELAY_STOP_EFFECT,
   /* Let an effect go, and free its slot. */
   RIB_PAD_RELAY_DROP_EFFECT
} rib_pad_relay_asking;

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
   /* We write this in the launcher before the game starts. */
   DIDEVICEINSTANCEA device;
   DWORD axis_count;
   DIDEVICEOBJECTINSTANCEA axes[RIB_PAD_RELAY_AXES];
   /* We write this in the launcher at each read: what GetDeviceState
    * returned, and the state from it. */
   HRESULT result;
   DIJOYSTATE2 state;
} rib_pad_relay_pad;

typedef struct {
   /* We write this in the launcher before the game starts. */
   DWORD pad_count;
   rib_pad_relay_pad pads[RIB_PAD_RELAY_PADS];
   rib_pad_relay_ask ask;
} rib_pad_relay;

#endif
