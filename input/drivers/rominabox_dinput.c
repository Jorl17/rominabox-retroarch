/* ROM-in-a-Box: DirectInput for a sandboxed Windows game
 * (rominabox_dinput.h). In the joypad driver we find the controllers, set
 * up their axes and rumble, and read each one once per frame. For the
 * controllers we relay through the launcher (rominabox_pad_relay.h) we
 * answer those calls through the relay, and pass every other call to
 * DirectInput itself. We request the controller states from the launcher
 * again when we read a controller for the second time since the last
 * request, so the reading once per frame in the driver costs one request
 * per frame. Setting an axis range or changing a rumble effect costs one
 * request each time we do it in the driver. */

#define WIN32_LEAN_AND_MEAN
#ifndef DIRECTINPUT_VERSION
#define DIRECTINPUT_VERSION 0x0800
#endif
#include <windows.h>
#include <dinput.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rominabox_dinput.h"
#include "../../rominabox_environment.h"
#include "../../rominabox_pad_relay.h"
#include "../../verbosity.h"

#if defined(UNICODE)
#error "the DirectInput stand-in answers RetroArch's ANSI DirectInput calls"
#endif

/* How long we wait for an answer from the launcher before we treat it as
 * gone and the controllers as lost. Listing the controllers takes longest. */
#define RELAY_WAIT_MS 5000

static rib_pad_relay *relay;
static HANDLE relay_request;
static HANDLE relay_reply;
static bool relay_gone;
/* The controllers whose state we have not passed to the driver since the
 * last read. */
static bool relay_fresh[RIB_PAD_RELAY_PADS];

/* Send the request in relay->ask to the launcher and return its answer. */
static HRESULT relay_exchange(void)
{
   if (relay_gone)
      return DIERR_INPUTLOST;
   SetEvent(relay_request);
   if (WaitForSingleObject(relay_reply, RELAY_WAIT_MS) != WAIT_OBJECT_0)
   {
      RARCH_ERR("[RIB] The launcher stopped answering for the game's controllers.\n");
      relay_gone = true;
      return DIERR_INPUTLOST;
   }
   return relay->ask.answer;
}

static void relay_read(void)
{
   unsigned i;
   relay->ask.what = RIB_PAD_RELAY_READ;
   if (FAILED(relay_exchange()))
      return;
   for (i = 0; i < RIB_PAD_RELAY_PADS; i++)
      relay_fresh[i] = true;
}

/* ---- a relayed controller's effect ------------------------------------- */

typedef struct
{
   IDirectInputEffect iface;
   LONG refs;
   unsigned pad;
   DWORD slot;
} relay_effect;

/* The parameters of `given` selected by `flags`, in the relay format. */
static void carry_effect(rib_pad_relay_effect *carried, LPCDIEFFECT given, DWORD flags)
{
   DWORD i;
   carried->flags = given->dwFlags;
   if (flags & DIEP_DURATION)
      carried->duration = given->dwDuration;
   if (flags & DIEP_SAMPLEPERIOD)
      carried->sample_period = given->dwSamplePeriod;
   if (flags & DIEP_GAIN)
      carried->gain = given->dwGain;
   if (flags & DIEP_TRIGGERBUTTON)
      carried->trigger_button = given->dwTriggerButton;
   if (flags & DIEP_TRIGGERREPEATINTERVAL)
      carried->trigger_repeat_interval = given->dwTriggerRepeatInterval;
   if (flags & DIEP_STARTDELAY)
      carried->start_delay = given->dwStartDelay;
   if (flags & (DIEP_AXES | DIEP_DIRECTION))
      carried->axis_count = given->cAxes;
   for (i = 0; i < given->cAxes && i < RIB_PAD_RELAY_EFFECT_AXES; i++)
   {
      if (flags & DIEP_AXES)
         carried->axes[i] = given->rgdwAxes[i];
      if (flags & DIEP_DIRECTION)
         carried->directions[i] = given->rglDirection[i];
   }
   if (flags & DIEP_ENVELOPE)
   {
      carried->has_envelope = given->lpEnvelope != NULL;
      if (given->lpEnvelope)
         carried->envelope = *given->lpEnvelope;
   }
   if (flags & DIEP_TYPESPECIFICPARAMS)
      carried->force = *(const DICONSTANTFORCE*)given->lpvTypeSpecificParams;
}

/* Whether we can relay the parameters of `given` selected by `flags`. We
 * relay a constant force along at most RIB_PAD_RELAY_EFFECT_AXES axes. */
static bool carriable(LPCDIEFFECT given, DWORD flags)
{
   if (given->dwSize != sizeof(DIEFFECT))
      return false;
   if ((flags & (DIEP_AXES | DIEP_DIRECTION)) && given->cAxes > RIB_PAD_RELAY_EFFECT_AXES)
      return false;
   if ((flags & DIEP_AXES) && given->cAxes && !given->rgdwAxes)
      return false;
   if ((flags & DIEP_DIRECTION) && given->cAxes && !given->rglDirection)
      return false;
   return !(flags & DIEP_TYPESPECIFICPARAMS)
      || (given->cbTypeSpecificParams == sizeof(DICONSTANTFORCE) && given->lpvTypeSpecificParams);
}

static HRESULT effect_ask(IDirectInputEffect *self, rib_pad_relay_asking what, DWORD flags)
{
   relay_effect *effect = (relay_effect*)self;
   relay->ask.what      = what;
   relay->ask.pad       = effect->pad;
   relay->ask.item      = effect->slot;
   relay->ask.flags     = flags;
   return relay_exchange();
}

static HRESULT STDMETHODCALLTYPE effect_query_interface(IDirectInputEffect *self, REFIID riid, void **out)
{
   if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IDirectInputEffect))
   {
      self->lpVtbl->AddRef(self);
      *out = self;
      return S_OK;
   }
   *out = NULL;
   return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE effect_add_ref(IDirectInputEffect *self)
{
   return (ULONG)InterlockedIncrement(&((relay_effect*)self)->refs);
}

static ULONG STDMETHODCALLTYPE effect_release(IDirectInputEffect *self)
{
   LONG refs = InterlockedDecrement(&((relay_effect*)self)->refs);
   if (refs == 0)
   {
      effect_ask(self, RIB_PAD_RELAY_DROP_EFFECT, 0);
      free(self);
   }
   return (ULONG)refs;
}

static HRESULT STDMETHODCALLTYPE effect_set_parameters(IDirectInputEffect *self, LPCDIEFFECT given, DWORD flags)
{
   if (!carriable(given, flags))
      return DIERR_INVALIDPARAM;
   carry_effect(&relay->ask.effect, given, flags);
   return effect_ask(self, RIB_PAD_RELAY_SET_EFFECT, flags);
}

static HRESULT STDMETHODCALLTYPE effect_stop(IDirectInputEffect *self)
{
   return effect_ask(self, RIB_PAD_RELAY_STOP_EFFECT, 0);
}

/* Calls on an effect that we never make in the joypad driver. */
static HRESULT STDMETHODCALLTYPE effect_initialize(IDirectInputEffect *self, HINSTANCE instance,
      DWORD version, REFGUID guid)
{
   (void)self;
   (void)instance;
   (void)version;
   (void)guid;
   return DIERR_UNSUPPORTED;
}

static HRESULT STDMETHODCALLTYPE effect_get_effect_guid(IDirectInputEffect *self, LPGUID guid)
{
   (void)self;
   *guid = GUID_ConstantForce;
   return DI_OK;
}

static HRESULT STDMETHODCALLTYPE effect_get_parameters(IDirectInputEffect *self, LPDIEFFECT out, DWORD flags)
{
   (void)self;
   (void)out;
   (void)flags;
   return DIERR_UNSUPPORTED;
}

static HRESULT STDMETHODCALLTYPE effect_start(IDirectInputEffect *self, DWORD iterations, DWORD flags)
{
   (void)self;
   (void)iterations;
   (void)flags;
   return DIERR_UNSUPPORTED;
}

static HRESULT STDMETHODCALLTYPE effect_get_effect_status(IDirectInputEffect *self, LPDWORD status)
{
   (void)self;
   (void)status;
   return DIERR_UNSUPPORTED;
}

static HRESULT STDMETHODCALLTYPE effect_download(IDirectInputEffect *self)
{
   (void)self;
   return DIERR_UNSUPPORTED;
}

static HRESULT STDMETHODCALLTYPE effect_unload(IDirectInputEffect *self)
{
   (void)self;
   return DIERR_UNSUPPORTED;
}

static HRESULT STDMETHODCALLTYPE effect_escape(IDirectInputEffect *self, LPDIEFFESCAPE escape)
{
   (void)self;
   (void)escape;
   return DIERR_UNSUPPORTED;
}

static IDirectInputEffectVtbl effect_vtbl = {
   .QueryInterface  = effect_query_interface,
   .AddRef          = effect_add_ref,
   .Release         = effect_release,
   .Initialize      = effect_initialize,
   .GetEffectGuid   = effect_get_effect_guid,
   .GetParameters   = effect_get_parameters,
   .SetParameters   = effect_set_parameters,
   .Start           = effect_start,
   .Stop            = effect_stop,
   .GetEffectStatus = effect_get_effect_status,
   .Download        = effect_download,
   .Unload          = effect_unload,
   .Escape          = effect_escape,
};

/* ---- a relayed controller ---------------------------------------------- */

typedef struct
{
   IDirectInputDevice8A iface;
   LONG refs;
   unsigned pad;
} relay_device;

static rib_pad_relay_pad *pad_of(IDirectInputDevice8A *self)
{
   return &relay->pads[((relay_device*)self)->pad];
}

static HRESULT STDMETHODCALLTYPE device_query_interface(IDirectInputDevice8A *self, REFIID riid, void **out)
{
   if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IDirectInputDevice8A))
   {
      self->lpVtbl->AddRef(self);
      *out = self;
      return S_OK;
   }
   *out = NULL;
   return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE device_add_ref(IDirectInputDevice8A *self)
{
   return (ULONG)InterlockedIncrement(&((relay_device*)self)->refs);
}

static ULONG STDMETHODCALLTYPE device_release(IDirectInputDevice8A *self)
{
   LONG refs = InterlockedDecrement(&((relay_device*)self)->refs);
   if (refs == 0)
      free(self);
   return (ULONG)refs;
}

/* In the joypad driver we request the absolute axes of a controller to set
 * their range, and we pass those through the relay. */
static HRESULT STDMETHODCALLTYPE device_enum_objects(IDirectInputDevice8A *self,
      LPDIENUMDEVICEOBJECTSCALLBACKA callback, LPVOID context, DWORD flags)
{
   rib_pad_relay_pad *pad = pad_of(self);
   DWORD i;
   if (flags != DIDFT_ALL && !(flags & DIDFT_ABSAXIS))
      return DI_OK;
   for (i = 0; i < pad->axis_count; i++)
      if (callback(&pad->axes[i], context) == DIENUM_STOP)
         break;
   return DI_OK;
}

/* An axis range, which we set on the controller in the launcher. */
static HRESULT STDMETHODCALLTYPE device_set_property(IDirectInputDevice8A *self,
      REFGUID property, LPCDIPROPHEADER header)
{
   const DIPROPRANGE *range = (const DIPROPRANGE*)header;
   if (     property != DIPROP_RANGE
         || header->dwHow != DIPH_BYID
         || header->dwSize != sizeof(DIPROPRANGE))
      return DIERR_UNSUPPORTED;
   relay->ask.what      = RIB_PAD_RELAY_SET_RANGE;
   relay->ask.pad       = ((relay_device*)self)->pad;
   relay->ask.item      = header->dwObj;
   relay->ask.range_min = range->lMin;
   relay->ask.range_max = range->lMax;
   return relay_exchange();
}

/* We open the controller in the launcher, so an acquire of the stand-in in
 * the game always succeeds. */
static HRESULT STDMETHODCALLTYPE device_acquire(IDirectInputDevice8A *self)
{
   (void)self;
   return DI_OK;
}

static HRESULT STDMETHODCALLTYPE device_unacquire(IDirectInputDevice8A *self)
{
   (void)self;
   return DI_OK;
}

static HRESULT STDMETHODCALLTYPE device_poll(IDirectInputDevice8A *self)
{
   (void)self;
   return relay_gone ? DIERR_INPUTLOST : DI_OK;
}

static HRESULT STDMETHODCALLTYPE device_get_device_state(IDirectInputDevice8A *self, DWORD size, LPVOID data)
{
   relay_device *device = (relay_device*)self;
   if (size != sizeof(DIJOYSTATE2))
      return DIERR_INVALIDPARAM;
   if (!relay_fresh[device->pad])
      relay_read();
   if (relay_gone)
      return DIERR_INPUTLOST;
   memcpy(data, &relay->pads[device->pad].state, sizeof(DIJOYSTATE2));
   relay_fresh[device->pad] = false;
   return relay->pads[device->pad].result;
}

/* We read the controller in the launcher as a DIJOYSTATE2, the format that
 * we set in the joypad driver. */
static HRESULT STDMETHODCALLTYPE device_set_data_format(IDirectInputDevice8A *self, LPCDIDATAFORMAT format)
{
   (void)self;
   return format->dwDataSize == sizeof(DIJOYSTATE2) ? DI_OK : DIERR_INVALIDPARAM;
}

/* We set this in the launcher, on the launcher window. */
static HRESULT STDMETHODCALLTYPE device_set_cooperative_level(IDirectInputDevice8A *self, HWND window, DWORD flags)
{
   (void)self;
   (void)window;
   (void)flags;
   return DI_OK;
}

static HRESULT STDMETHODCALLTYPE device_get_device_info(IDirectInputDevice8A *self, LPDIDEVICEINSTANCEA info)
{
   if (info->dwSize != sizeof(DIDEVICEINSTANCEA))
      return DIERR_INVALIDPARAM;
   memcpy(info, &pad_of(self)->device, sizeof(DIDEVICEINSTANCEA));
   return DI_OK;
}

/* Calls on a controller that we never make in the joypad driver. */
static HRESULT STDMETHODCALLTYPE device_get_capabilities(IDirectInputDevice8A *self, LPDIDEVCAPS caps)
{
   (void)self;
   (void)caps;
   return DIERR_UNSUPPORTED;
}

static HRESULT STDMETHODCALLTYPE device_get_property(IDirectInputDevice8A *self, REFGUID property, LPDIPROPHEADER header)
{
   (void)self;
   (void)property;
   (void)header;
   return DIERR_UNSUPPORTED;
}

static HRESULT STDMETHODCALLTYPE device_get_device_data(IDirectInputDevice8A *self, DWORD size,
      LPDIDEVICEOBJECTDATA data, LPDWORD count, DWORD flags)
{
   (void)self;
   (void)size;
   (void)data;
   (void)count;
   (void)flags;
   return DIERR_UNSUPPORTED;
}

static HRESULT STDMETHODCALLTYPE device_set_event_notification(IDirectInputDevice8A *self, HANDLE event)
{
   (void)self;
   (void)event;
   return DIERR_UNSUPPORTED;
}

static HRESULT STDMETHODCALLTYPE device_get_object_info(IDirectInputDevice8A *self,
      LPDIDEVICEOBJECTINSTANCEA info, DWORD object, DWORD how)
{
   (void)self;
   (void)info;
   (void)object;
   (void)how;
   return DIERR_UNSUPPORTED;
}

static HRESULT STDMETHODCALLTYPE device_run_control_panel(IDirectInputDevice8A *self, HWND owner, DWORD flags)
{
   (void)self;
   (void)owner;
   (void)flags;
   return DIERR_UNSUPPORTED;
}

static HRESULT STDMETHODCALLTYPE device_initialize(IDirectInputDevice8A *self, HINSTANCE instance,
      DWORD version, REFGUID guid)
{
   (void)self;
   (void)instance;
   (void)version;
   (void)guid;
   return DIERR_UNSUPPORTED;
}

/* A rumble effect that we create on the controller in the launcher. It is a
 * constant force, the only kind we create in the joypad driver. */
static HRESULT STDMETHODCALLTYPE device_create_effect(IDirectInputDevice8A *self, REFGUID guid,
      LPCDIEFFECT given, LPDIRECTINPUTEFFECT *out, LPUNKNOWN outer)
{
   const DWORD every = DIEP_DURATION | DIEP_SAMPLEPERIOD | DIEP_GAIN | DIEP_TRIGGERBUTTON
      | DIEP_TRIGGERREPEATINTERVAL | DIEP_STARTDELAY | DIEP_AXES | DIEP_DIRECTION | DIEP_ENVELOPE
      | DIEP_TYPESPECIFICPARAMS;
   relay_effect *effect;
   HRESULT made;
   *out = NULL;
   if (outer || !IsEqualGUID(guid, &GUID_ConstantForce))
      return DIERR_UNSUPPORTED;
   if (!given || !carriable(given, every))
      return DIERR_INVALIDPARAM;
   memset(&relay->ask.effect, 0, sizeof(relay->ask.effect));
   carry_effect(&relay->ask.effect, given, every);
   relay->ask.what = RIB_PAD_RELAY_MAKE_EFFECT;
   relay->ask.pad  = ((relay_device*)self)->pad;
   if (FAILED(made = relay_exchange()))
      return made;
   if (!(effect = (relay_effect*)calloc(1, sizeof(*effect))))
   {
      relay->ask.what = RIB_PAD_RELAY_DROP_EFFECT;
      relay_exchange();
      return DIERR_OUTOFMEMORY;
   }
   effect->iface.lpVtbl = &effect_vtbl;
   effect->refs         = 1;
   effect->pad          = ((relay_device*)self)->pad;
   effect->slot         = relay->ask.item;
   *out                 = &effect->iface;
   return made;
}

static HRESULT STDMETHODCALLTYPE device_enum_effects(IDirectInputDevice8A *self,
      LPDIENUMEFFECTSCALLBACKA callback, LPVOID context, DWORD type)
{
   (void)self;
   (void)callback;
   (void)context;
   (void)type;
   return DIERR_UNSUPPORTED;
}

static HRESULT STDMETHODCALLTYPE device_get_effect_info(IDirectInputDevice8A *self, LPDIEFFECTINFOA info, REFGUID guid)
{
   (void)self;
   (void)info;
   (void)guid;
   return DIERR_UNSUPPORTED;
}

static HRESULT STDMETHODCALLTYPE device_get_force_feedback_state(IDirectInputDevice8A *self, LPDWORD out)
{
   (void)self;
   (void)out;
   return DIERR_UNSUPPORTED;
}

static HRESULT STDMETHODCALLTYPE device_send_force_feedback_command(IDirectInputDevice8A *self, DWORD flags)
{
   (void)self;
   (void)flags;
   return DIERR_UNSUPPORTED;
}

static HRESULT STDMETHODCALLTYPE device_enum_created_effect_objects(IDirectInputDevice8A *self,
      LPDIENUMCREATEDEFFECTOBJECTSCALLBACK callback, LPVOID context, DWORD flags)
{
   (void)self;
   (void)callback;
   (void)context;
   (void)flags;
   return DIERR_UNSUPPORTED;
}

static HRESULT STDMETHODCALLTYPE device_escape(IDirectInputDevice8A *self, LPDIEFFESCAPE escape)
{
   (void)self;
   (void)escape;
   return DIERR_UNSUPPORTED;
}

static HRESULT STDMETHODCALLTYPE device_send_device_data(IDirectInputDevice8A *self, DWORD size,
      LPCDIDEVICEOBJECTDATA data, LPDWORD count, DWORD flags)
{
   (void)self;
   (void)size;
   (void)data;
   (void)count;
   (void)flags;
   return DIERR_UNSUPPORTED;
}

static HRESULT STDMETHODCALLTYPE device_enum_effects_in_file(IDirectInputDevice8A *self, LPCSTR file,
      LPDIENUMEFFECTSINFILECALLBACK callback, LPVOID context, DWORD flags)
{
   (void)self;
   (void)file;
   (void)callback;
   (void)context;
   (void)flags;
   return DIERR_UNSUPPORTED;
}

static HRESULT STDMETHODCALLTYPE device_write_effect_to_file(IDirectInputDevice8A *self, LPCSTR file,
      DWORD entries, LPDIFILEEFFECT effects, DWORD flags)
{
   (void)self;
   (void)file;
   (void)entries;
   (void)effects;
   (void)flags;
   return DIERR_UNSUPPORTED;
}

static HRESULT STDMETHODCALLTYPE device_build_action_map(IDirectInputDevice8A *self,
      LPDIACTIONFORMATA format, LPCSTR user, DWORD flags)
{
   (void)self;
   (void)format;
   (void)user;
   (void)flags;
   return DIERR_UNSUPPORTED;
}

static HRESULT STDMETHODCALLTYPE device_set_action_map(IDirectInputDevice8A *self,
      LPDIACTIONFORMATA format, LPCSTR user, DWORD flags)
{
   (void)self;
   (void)format;
   (void)user;
   (void)flags;
   return DIERR_UNSUPPORTED;
}

static HRESULT STDMETHODCALLTYPE device_get_image_info(IDirectInputDevice8A *self,
      LPDIDEVICEIMAGEINFOHEADERA header)
{
   (void)self;
   (void)header;
   return DIERR_UNSUPPORTED;
}

static IDirectInputDevice8AVtbl device_vtbl = {
   .QueryInterface           = device_query_interface,
   .AddRef                   = device_add_ref,
   .Release                  = device_release,
   .GetCapabilities          = device_get_capabilities,
   .EnumObjects              = device_enum_objects,
   .GetProperty              = device_get_property,
   .SetProperty              = device_set_property,
   .Acquire                  = device_acquire,
   .Unacquire                = device_unacquire,
   .GetDeviceState           = device_get_device_state,
   .GetDeviceData            = device_get_device_data,
   .SetDataFormat            = device_set_data_format,
   .SetEventNotification     = device_set_event_notification,
   .SetCooperativeLevel      = device_set_cooperative_level,
   .GetObjectInfo            = device_get_object_info,
   .GetDeviceInfo            = device_get_device_info,
   .RunControlPanel          = device_run_control_panel,
   .Initialize               = device_initialize,
   .CreateEffect             = device_create_effect,
   .EnumEffects              = device_enum_effects,
   .GetEffectInfo            = device_get_effect_info,
   .GetForceFeedbackState    = device_get_force_feedback_state,
   .SendForceFeedbackCommand = device_send_force_feedback_command,
   .EnumCreatedEffectObjects = device_enum_created_effect_objects,
   .Escape                   = device_escape,
   .Poll                     = device_poll,
   .SendDeviceData           = device_send_device_data,
   .EnumEffectsInFile        = device_enum_effects_in_file,
   .WriteEffectToFile        = device_write_effect_to_file,
   .BuildActionMap           = device_build_action_map,
   .SetActionMap             = device_set_action_map,
   .GetImageInfo             = device_get_image_info,
};

/* ---- DirectInput, with the relayed controllers ------------------------- */

typedef struct
{
   IDirectInput8A iface;
   LONG refs;
   IDirectInput8A *real;
} relay_input;

static IDirectInput8A *real_of(IDirectInput8A *self)
{
   return ((relay_input*)self)->real;
}

/* The relayed controller with the DirectInput `guid`, or -1. */
static int relayed(REFGUID guid)
{
   DWORD i;
   for (i = 0; i < relay->pad_count; i++)
      if (IsEqualGUID(guid, &relay->pads[i].device.guidInstance))
         return (int)i;
   return -1;
}

static HRESULT STDMETHODCALLTYPE input_query_interface(IDirectInput8A *self, REFIID riid, void **out)
{
   if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IDirectInput8A))
   {
      self->lpVtbl->AddRef(self);
      *out = self;
      return S_OK;
   }
   *out = NULL;
   return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE input_add_ref(IDirectInput8A *self)
{
   return (ULONG)InterlockedIncrement(&((relay_input*)self)->refs);
}

static ULONG STDMETHODCALLTYPE input_release(IDirectInput8A *self)
{
   relay_input *input = (relay_input*)self;
   LONG refs = InterlockedDecrement(&input->refs);
   if (refs == 0)
   {
      IDirectInput8_Release(input->real);
      free(input);
   }
   return (ULONG)refs;
}

static HRESULT STDMETHODCALLTYPE input_create_device(IDirectInput8A *self, REFGUID guid,
      LPDIRECTINPUTDEVICE8A *out, LPUNKNOWN outer)
{
   int pad = relayed(guid);
   relay_device *device;
   if (pad < 0)
      return IDirectInput8_CreateDevice(real_of(self), guid, out, outer);
   if (!(device = (relay_device*)calloc(1, sizeof(*device))))
      return DIERR_OUTOFMEMORY;
   device->iface.lpVtbl = &device_vtbl;
   device->refs         = 1;
   device->pad          = (unsigned)pad;
   *out                 = &device->iface;
   RARCH_LOG("[RIB] Controller \"%s\" opened through the launcher.\n",
         relay->pads[pad].device.tszProductName);
   return DI_OK;
}

/* We list the controllers in the joypad driver at start, and again, after
 * releasing all, whenever a device is added or removed. We list them afresh
 * in the launcher each time, and every controller listed is attached. */
static HRESULT STDMETHODCALLTYPE input_enum_devices(IDirectInput8A *self, DWORD type,
      LPDIENUMDEVICESCALLBACKA callback, LPVOID context, DWORD flags)
{
   DWORD i;
   HRESULT listed;
   if (type != DI8DEVCLASS_GAMECTRL)
      return IDirectInput8_EnumDevices(real_of(self), type, callback, context, flags);
   relay->ask.what = RIB_PAD_RELAY_LIST;
   if (FAILED(listed = relay_exchange()))
      return listed;
   for (i = 0; i < RIB_PAD_RELAY_PADS; i++)
      relay_fresh[i] = false;
   RARCH_LOG("[RIB] %lu controller(s) come through the launcher.\n", (unsigned long)relay->pad_count);
   for (i = 0; i < relay->pad_count; i++)
      if (callback(&relay->pads[i].device, context) == DIENUM_STOP)
         break;
   return DI_OK;
}

static HRESULT STDMETHODCALLTYPE input_get_device_status(IDirectInput8A *self, REFGUID guid)
{
   if (relayed(guid) >= 0)
      return DI_OK;
   return IDirectInput8_GetDeviceStatus(real_of(self), guid);
}

static HRESULT STDMETHODCALLTYPE input_run_control_panel(IDirectInput8A *self, HWND owner, DWORD flags)
{
   return IDirectInput8_RunControlPanel(real_of(self), owner, flags);
}

static HRESULT STDMETHODCALLTYPE input_initialize(IDirectInput8A *self, HINSTANCE instance, DWORD version)
{
   return IDirectInput8_Initialize(real_of(self), instance, version);
}

static HRESULT STDMETHODCALLTYPE input_find_device(IDirectInput8A *self, REFGUID guid, LPCSTR name, LPGUID out)
{
   return IDirectInput8_FindDevice(real_of(self), guid, name, out);
}

static HRESULT STDMETHODCALLTYPE input_enum_devices_by_semantics(IDirectInput8A *self, LPCSTR user,
      LPDIACTIONFORMATA format, LPDIENUMDEVICESBYSEMANTICSCBA callback, LPVOID context, DWORD flags)
{
   return IDirectInput8_EnumDevicesBySemantics(real_of(self), user, format, callback, context, flags);
}

static HRESULT STDMETHODCALLTYPE input_configure_devices(IDirectInput8A *self,
      LPDICONFIGUREDEVICESCALLBACK callback, LPDICONFIGUREDEVICESPARAMSA params, DWORD flags, LPVOID context)
{
   return IDirectInput8_ConfigureDevices(real_of(self), callback, params, flags, context);
}

static IDirectInput8AVtbl input_vtbl = {
   .QueryInterface         = input_query_interface,
   .AddRef                 = input_add_ref,
   .Release                = input_release,
   .CreateDevice           = input_create_device,
   .EnumDevices            = input_enum_devices,
   .GetDeviceStatus        = input_get_device_status,
   .RunControlPanel        = input_run_control_panel,
   .Initialize             = input_initialize,
   .FindDevice             = input_find_device,
   .EnumDevicesBySemantics = input_enum_devices_by_semantics,
   .ConfigureDevices       = input_configure_devices,
};

/* The relay from the launcher, which we map once for the whole game. */
static bool relay_open(void)
{
   char *handles;
   unsigned long long block = 0;
   unsigned long long request = 0;
   unsigned long long reply = 0;
   if (relay)
      return true;
   if (!(handles = rib_environment(RIB_ENV_PAD_RELAY)))
      return false;
   if (     sscanf(handles, "%llu,%llu,%llu", &block, &request, &reply) != 3
         || !(relay = (rib_pad_relay*)MapViewOfFile((HANDLE)(uintptr_t)block,
               FILE_MAP_ALL_ACCESS, 0, 0, sizeof(rib_pad_relay))))
   {
      RARCH_ERR("[RIB] The controller relay the launcher named cannot be used: %s.\n", handles);
      free(handles);
      return false;
   }
   free(handles);
   relay_request = (HANDLE)(uintptr_t)request;
   relay_reply   = (HANDLE)(uintptr_t)reply;
   return true;
}

LPDIRECTINPUT8 rib_dinput_for_game(LPDIRECTINPUT8 real)
{
   relay_input *input;
   if (!real || !relay_open())
      return real;
   if (!(input = (relay_input*)calloc(1, sizeof(*input))))
      return real;
   input->iface.lpVtbl = &input_vtbl;
   input->refs         = 1;
   input->real         = real;
   return &input->iface;
}
