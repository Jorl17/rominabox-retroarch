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
 * (input/drivers/rominabox_dinput.c) to the RetroArch joypad driver unchanged.
 *
 * From the game we set the request event and wait for the reply event, and
 * between the two we read every controller in the launcher. Windows only.
 * We also include this in the launcher, which we build without RetroArch. */
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

typedef struct {
   /* We write this in the launcher before the game starts. */
   DIDEVICEINSTANCEA device;
   DWORD axis_count;
   DIDEVICEOBJECTINSTANCEA axes[RIB_PAD_RELAY_AXES];
   /* We write this in the game: the range set on each axis, both 0 until
    * one is set. In the launcher we set the same range on the controller. */
   LONG range_min[RIB_PAD_RELAY_AXES];
   LONG range_max[RIB_PAD_RELAY_AXES];
   /* We write this in the launcher at each read: what GetDeviceState
    * returned, and the state from it. */
   HRESULT result;
   DIJOYSTATE2 state;
} rib_pad_relay_pad;

typedef struct {
   /* We write this in the launcher before the game starts. */
   DWORD pad_count;
   rib_pad_relay_pad pads[RIB_PAD_RELAY_PADS];
} rib_pad_relay;

#endif
