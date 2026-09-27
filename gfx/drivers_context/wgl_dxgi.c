/* ROM-in-a-Box: presenting fullscreen on Windows through DXGI (wgl_dxgi.h). */
#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
/* Our copies of the interface identifiers we request, so we link the
 * player with neither dxguid nor Direct3D. We load d3d11.dll only when a
 * game goes fullscreen. */
#include <initguid.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <GL/gl.h>
#include <stdlib.h>

#include "wgl_dxgi.h"
#include "../../verbosity.h"

#define DXGI_GL_FRAMEBUFFER             0x8D40
#define DXGI_GL_READ_FRAMEBUFFER        0x8CA8
#define DXGI_GL_DRAW_FRAMEBUFFER        0x8CA9
#define DXGI_GL_RENDERBUFFER            0x8D41
#define DXGI_GL_COLOR_ATTACHMENT0       0x8CE0
#define DXGI_GL_DEPTH_STENCIL_ATTACHMENT 0x821A
#define DXGI_GL_DEPTH24_STENCIL8        0x88F0
#define DXGI_GL_RGBA8                   0x8058
#define DXGI_GL_FRAMEBUFFER_COMPLETE    0x8CD5
#define DXGI_GL_FRAMEBUFFER_BINDING     0x8CA6
#define DXGI_WGL_ACCESS_WRITE_DISCARD   0x0002
/* The swapchain's buffers and the shared texture. */
#define DXGI_PICTURE_FORMAT DXGI_FORMAT_R8G8B8A8_UNORM

typedef HRESULT (WINAPI *create_device_t)(IDXGIAdapter *, D3D_DRIVER_TYPE, HMODULE, UINT,
      const D3D_FEATURE_LEVEL *, UINT, UINT, ID3D11Device **, D3D_FEATURE_LEVEL *,
      ID3D11DeviceContext **);

struct wgl_dxgi
{
   HMODULE library;
   HWND window;
   ID3D11Device *device;
   ID3D11DeviceContext *context;
   IDXGISwapChain1 *swapchain;
   /* Shared between OpenGL and Direct3D: a texture and a framebuffer on it. */
   ID3D11Texture2D *shared;
   HANDLE interop;
   HANDLE object;
   GLuint shared_texture;
   GLuint shared_framebuffer;
   /* Where we draw the picture with OpenGL, upside down for Direct3D. */
   GLuint framebuffer;
   GLuint color;
   GLuint depth_stencil;
   unsigned width;
   unsigned height;

   HANDLE (WINAPI *open_device)(void *);
   BOOL (WINAPI *close_device)(HANDLE);
   HANDLE (WINAPI *register_object)(HANDLE, void *, GLuint, GLenum, GLenum);
   BOOL (WINAPI *unregister_object)(HANDLE, HANDLE);
   BOOL (WINAPI *lock_objects)(HANDLE, GLint, HANDLE *);
   BOOL (WINAPI *unlock_objects)(HANDLE, GLint, HANDLE *);
   void (APIENTRY *gen_framebuffers)(GLsizei, GLuint *);
   void (APIENTRY *delete_framebuffers)(GLsizei, const GLuint *);
   void (APIENTRY *bind_framebuffer)(GLenum, GLuint);
   void (APIENTRY *framebuffer_texture)(GLenum, GLenum, GLenum, GLuint, GLint);
   void (APIENTRY *gen_renderbuffers)(GLsizei, GLuint *);
   void (APIENTRY *delete_renderbuffers)(GLsizei, const GLuint *);
   void (APIENTRY *bind_renderbuffer)(GLenum, GLuint);
   void (APIENTRY *renderbuffer_storage)(GLenum, GLenum, GLsizei, GLsizei);
   void (APIENTRY *framebuffer_renderbuffer)(GLenum, GLenum, GLenum, GLuint);
   GLenum (APIENTRY *framebuffer_status)(GLenum);
   void (APIENTRY *blit_framebuffer)(GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint,
         GLbitfield, GLenum);
};

static bool dxgi_functions(wgl_dxgi_t *dxgi)
{
#define DXGI_FUNCTION(field, name) \
   (*(PROC*)&dxgi->field = wglGetProcAddress(name)) != NULL
   return DXGI_FUNCTION(open_device, "wglDXOpenDeviceNV")
      && DXGI_FUNCTION(close_device, "wglDXCloseDeviceNV")
      && DXGI_FUNCTION(register_object, "wglDXRegisterObjectNV")
      && DXGI_FUNCTION(unregister_object, "wglDXUnregisterObjectNV")
      && DXGI_FUNCTION(lock_objects, "wglDXLockObjectsNV")
      && DXGI_FUNCTION(unlock_objects, "wglDXUnlockObjectsNV")
      && DXGI_FUNCTION(gen_framebuffers, "glGenFramebuffers")
      && DXGI_FUNCTION(delete_framebuffers, "glDeleteFramebuffers")
      && DXGI_FUNCTION(bind_framebuffer, "glBindFramebuffer")
      && DXGI_FUNCTION(framebuffer_texture, "glFramebufferTexture2D")
      && DXGI_FUNCTION(gen_renderbuffers, "glGenRenderbuffers")
      && DXGI_FUNCTION(delete_renderbuffers, "glDeleteRenderbuffers")
      && DXGI_FUNCTION(bind_renderbuffer, "glBindRenderbuffer")
      && DXGI_FUNCTION(renderbuffer_storage, "glRenderbufferStorage")
      && DXGI_FUNCTION(framebuffer_renderbuffer, "glFramebufferRenderbuffer")
      && DXGI_FUNCTION(framebuffer_status, "glCheckFramebufferStatus")
      && DXGI_FUNCTION(blit_framebuffer, "glBlitFramebuffer");
#undef DXGI_FUNCTION
}

/* Release everything that has the size of the window. */
static void dxgi_release_targets(wgl_dxgi_t *dxgi)
{
   if (dxgi->object)
      dxgi->unregister_object(dxgi->interop, dxgi->object);
   dxgi->object = NULL;
   if (dxgi->shared_framebuffer)
      dxgi->delete_framebuffers(1, &dxgi->shared_framebuffer);
   if (dxgi->framebuffer)
      dxgi->delete_framebuffers(1, &dxgi->framebuffer);
   if (dxgi->shared_texture)
      glDeleteTextures(1, &dxgi->shared_texture);
   if (dxgi->color)
      dxgi->delete_renderbuffers(1, &dxgi->color);
   if (dxgi->depth_stencil)
      dxgi->delete_renderbuffers(1, &dxgi->depth_stencil);
   dxgi->shared_framebuffer = dxgi->framebuffer = dxgi->shared_texture = 0;
   dxgi->color = dxgi->depth_stencil = 0;
   if (dxgi->shared)
      ID3D11Texture2D_Release(dxgi->shared);
   dxgi->shared = NULL;
}

/* The shared texture and the framebuffers, at `width` by `height`. */
static bool dxgi_make_targets(wgl_dxgi_t *dxgi, unsigned width, unsigned height)
{
   D3D11_TEXTURE2D_DESC texture = {0};
   GLint bound = 0;
   bool complete;

   texture.Width = width;
   texture.Height = height;
   texture.MipLevels = 1;
   texture.ArraySize = 1;
   texture.Format = DXGI_PICTURE_FORMAT;
   texture.SampleDesc.Count = 1;
   texture.Usage = D3D11_USAGE_DEFAULT;
   texture.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
   if (FAILED(ID3D11Device_CreateTexture2D(dxgi->device, &texture, NULL, &dxgi->shared)))
      return false;
   glGenTextures(1, &dxgi->shared_texture);
   dxgi->object = dxgi->register_object(dxgi->interop, dxgi->shared, dxgi->shared_texture,
         GL_TEXTURE_2D, DXGI_WGL_ACCESS_WRITE_DISCARD);
   if (!dxgi->object)
      return false;

   glGetIntegerv(DXGI_GL_FRAMEBUFFER_BINDING, &bound);
   dxgi->lock_objects(dxgi->interop, 1, &dxgi->object);
   dxgi->gen_framebuffers(1, &dxgi->shared_framebuffer);
   dxgi->bind_framebuffer(DXGI_GL_FRAMEBUFFER, dxgi->shared_framebuffer);
   dxgi->framebuffer_texture(DXGI_GL_FRAMEBUFFER, DXGI_GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
         dxgi->shared_texture, 0);
   complete = dxgi->framebuffer_status(DXGI_GL_FRAMEBUFFER) == DXGI_GL_FRAMEBUFFER_COMPLETE;
   dxgi->unlock_objects(dxgi->interop, 1, &dxgi->object);

   /* The framebuffer for the window, with colour, depth and stencil
    * buffers, as in the pixel format of a window. */
   dxgi->gen_renderbuffers(1, &dxgi->color);
   dxgi->bind_renderbuffer(DXGI_GL_RENDERBUFFER, dxgi->color);
   dxgi->renderbuffer_storage(DXGI_GL_RENDERBUFFER, DXGI_GL_RGBA8, width, height);
   dxgi->gen_renderbuffers(1, &dxgi->depth_stencil);
   dxgi->bind_renderbuffer(DXGI_GL_RENDERBUFFER, dxgi->depth_stencil);
   dxgi->renderbuffer_storage(DXGI_GL_RENDERBUFFER, DXGI_GL_DEPTH24_STENCIL8, width, height);
   dxgi->bind_renderbuffer(DXGI_GL_RENDERBUFFER, 0);
   dxgi->gen_framebuffers(1, &dxgi->framebuffer);
   dxgi->bind_framebuffer(DXGI_GL_FRAMEBUFFER, dxgi->framebuffer);
   dxgi->framebuffer_renderbuffer(DXGI_GL_FRAMEBUFFER, DXGI_GL_COLOR_ATTACHMENT0,
         DXGI_GL_RENDERBUFFER, dxgi->color);
   dxgi->framebuffer_renderbuffer(DXGI_GL_FRAMEBUFFER, DXGI_GL_DEPTH_STENCIL_ATTACHMENT,
         DXGI_GL_RENDERBUFFER, dxgi->depth_stencil);
   complete = complete
      && dxgi->framebuffer_status(DXGI_GL_FRAMEBUFFER) == DXGI_GL_FRAMEBUFFER_COMPLETE;
   dxgi->bind_framebuffer(DXGI_GL_FRAMEBUFFER, (GLuint)bound);

   dxgi->width = width;
   dxgi->height = height;
   return complete;
}

static void dxgi_client_size(HWND window, unsigned *width, unsigned *height)
{
   RECT client = {0};
   GetClientRect(window, &client);
   *width = client.right > client.left ? (unsigned)(client.right - client.left) : 1;
   *height = client.bottom > client.top ? (unsigned)(client.bottom - client.top) : 1;
}

wgl_dxgi_t *wgl_dxgi_new(HWND window)
{
   wgl_dxgi_t *dxgi = (wgl_dxgi_t*)calloc(1, sizeof(*dxgi));
   create_device_t create_device;
   IDXGIDevice *device = NULL;
   IDXGIAdapter *adapter = NULL;
   IDXGIFactory2 *factory = NULL;
   DXGI_SWAP_CHAIN_DESC1 description = {0};
   unsigned width, height;
   const char *missing = NULL;

   if (!dxgi)
      return NULL;
   dxgi->window = window;
   dxgi_client_size(window, &width, &height);

   if (!dxgi_functions(dxgi))
      missing = "WGL_NV_DX_interop";
   else if (!(dxgi->library = LoadLibraryW(L"d3d11.dll"))
         || !(create_device = (create_device_t)(void*)GetProcAddress(dxgi->library, "D3D11CreateDevice")))
      missing = "Direct3D 11";
   else if (FAILED(create_device(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, NULL, 0,
            D3D11_SDK_VERSION, &dxgi->device, NULL, &dxgi->context))
         || FAILED(ID3D11Device_QueryInterface(dxgi->device, &IID_IDXGIDevice, (void**)&device))
         || FAILED(IDXGIDevice_GetAdapter(device, &adapter))
         || FAILED(IDXGIAdapter_GetParent(adapter, &IID_IDXGIFactory2, (void**)&factory)))
      missing = "a Direct3D 11 device";
   else
   {
      description.Width = width;
      description.Height = height;
      description.Format = DXGI_PICTURE_FORMAT;
      description.SampleDesc.Count = 1;
      description.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
      description.BufferCount = 2;
      description.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
      if (FAILED(IDXGIFactory2_CreateSwapChainForHwnd(factory, (IUnknown*)dxgi->device, window,
               &description, NULL, NULL, &dxgi->swapchain)))
         missing = "a flip-model swapchain";
      /* Keep Alt+Enter for the game. Otherwise the keys would switch DXGI
       * to its exclusive fullscreen. */
      else if (FAILED(IDXGIFactory2_MakeWindowAssociation(factory, window,
               DXGI_MWA_NO_ALT_ENTER | DXGI_MWA_NO_WINDOW_CHANGES)))
         missing = "the window's keys to itself";
      else if (!(dxgi->interop = dxgi->open_device(dxgi->device)))
         missing = "OpenGL's view of the Direct3D device";
      else if (!dxgi_make_targets(dxgi, width, height))
         missing = "a framebuffer OpenGL shares with Direct3D";
   }
   if (factory)
      IDXGIFactory2_Release(factory);
   if (adapter)
      IDXGIAdapter_Release(adapter);
   if (device)
      IDXGIDevice_Release(device);

   if (missing)
   {
      RARCH_WARN("[RIB] Fullscreen presents with OpenGL: no %s.\n", missing);
      wgl_dxgi_free(dxgi);
      return NULL;
   }
   RARCH_LOG("[RIB] Fullscreen is presented through DXGI, %ux%u.\n", width, height);
   return dxgi;
}

unsigned wgl_dxgi_framebuffer(const wgl_dxgi_t *dxgi)
{
   return dxgi->framebuffer;
}

void wgl_dxgi_present(wgl_dxgi_t *dxgi, int interval)
{
   ID3D11Texture2D *back = NULL;
   GLboolean scissor = glIsEnabled(GL_SCISSOR_TEST);
   unsigned width, height;

   dxgi->lock_objects(dxgi->interop, 1, &dxgi->object);
   dxgi->bind_framebuffer(DXGI_GL_READ_FRAMEBUFFER, dxgi->framebuffer);
   dxgi->bind_framebuffer(DXGI_GL_DRAW_FRAMEBUFFER, dxgi->shared_framebuffer);
   if (scissor)
      glDisable(GL_SCISSOR_TEST);
   /* Rows go bottom up in OpenGL and top down in Direct3D. */
   dxgi->blit_framebuffer(0, 0, dxgi->width, dxgi->height,
         0, dxgi->height, dxgi->width, 0, GL_COLOR_BUFFER_BIT, GL_NEAREST);
   if (scissor)
      glEnable(GL_SCISSOR_TEST);
   dxgi->bind_framebuffer(DXGI_GL_FRAMEBUFFER, dxgi->framebuffer);
   dxgi->unlock_objects(dxgi->interop, 1, &dxgi->object);

   if (SUCCEEDED(IDXGISwapChain1_GetBuffer(dxgi->swapchain, 0, &IID_ID3D11Texture2D, (void**)&back)))
   {
      ID3D11DeviceContext_CopyResource(dxgi->context, (ID3D11Resource*)back,
            (ID3D11Resource*)dxgi->shared);
      ID3D11Texture2D_Release(back);
   }
   IDXGISwapChain1_Present(dxgi->swapchain, interval < 0 ? 0 : interval > 4 ? 4 : interval, 0);

   /* When the window changes size, we resize at the next frame. */
   dxgi_client_size(dxgi->window, &width, &height);
   if (width != dxgi->width || height != dxgi->height)
   {
      dxgi_release_targets(dxgi);
      if (FAILED(IDXGISwapChain1_ResizeBuffers(dxgi->swapchain, 0, width, height,
               DXGI_FORMAT_UNKNOWN, 0))
            || !dxgi_make_targets(dxgi, width, height))
         RARCH_ERR("[RIB] The DXGI picture could not follow the window to %ux%u.\n", width, height);
      dxgi->bind_framebuffer(DXGI_GL_FRAMEBUFFER, dxgi->framebuffer);
   }
}

void wgl_dxgi_free(wgl_dxgi_t *dxgi)
{
   if (!dxgi)
      return;
   if (dxgi->interop)
   {
      dxgi_release_targets(dxgi);
      dxgi->close_device(dxgi->interop);
   }
   else if (dxgi->shared)
      ID3D11Texture2D_Release(dxgi->shared);
   if (dxgi->swapchain)
      IDXGISwapChain1_Release(dxgi->swapchain);
   if (dxgi->context)
      ID3D11DeviceContext_Release(dxgi->context);
   if (dxgi->device)
      ID3D11Device_Release(dxgi->device);
   if (dxgi->library)
      FreeLibrary(dxgi->library);
   free(dxgi);
}
