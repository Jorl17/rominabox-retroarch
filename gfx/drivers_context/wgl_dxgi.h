/* ROM-in-a-Box: presenting fullscreen on Windows through DXGI.
 *
 * With the NVIDIA OpenGL driver, a window that covers the screen is in
 * exclusive mode, and on an HDR desktop the display switches to SDR while
 * the game is fullscreen. Windows composes a DXGI flip-model swapchain like
 * any other window, so the display stays in HDR.
 *
 * We draw everything with OpenGL into a framebuffer of this presenter. At
 * each swap we copy that picture, upright, into a Direct3D texture shared
 * with OpenGL (WGL_NV_DX_interop2), and the texture into the swapchain. In
 * a window we present through the window itself, so we start Direct3D only
 * for fullscreen. */
#ifndef WGL_DXGI_H
#define WGL_DXGI_H

#include <windows.h>
#include <boolean.h>

typedef struct wgl_dxgi wgl_dxgi_t;

/* Return a presenter for `window`, for the OpenGL context current on this
 * thread, or NULL, after logging the reason, where presenting that way is
 * not possible. We then present the game as in a window. */
wgl_dxgi_t *wgl_dxgi_new(HWND window);

/* The framebuffer we draw the window's picture into with OpenGL. */
unsigned wgl_dxgi_framebuffer(const wgl_dxgi_t *dxgi);

/* Show what we drew, after `interval` vertical blanks (0: at once). */
void wgl_dxgi_present(wgl_dxgi_t *dxgi, int interval);

/* Free the presenter while the OpenGL context is still current. */
void wgl_dxgi_free(wgl_dxgi_t *dxgi);

/* The framebuffer that represents the window, which is the presenter's
 * while we present through one, or else 0. We define it in wgl_ctx.c. */
unsigned wgl_window_framebuffer(void);

#endif
