/* ROM-in-a-Box: the window we draw into with the two OpenGL drivers in
 * RetroArch, defined once for gl (gfx/drivers/gl2.c), with GLSL shaders,
 * and glcore (gfx/drivers/gl3.c), with slang shaders. We include this file
 * in each driver after its GL headers, and get the window from it instead
 * of assuming the default framebuffer. */
#ifndef GL_WINDOW_H
#define GL_WINDOW_H

/* The framebuffer that represents the window. When fullscreen on Windows
 * we draw into the one we present through DXGI (wgl_dxgi.h), so HDR stays
 * on. Otherwise we draw into the default one. On iOS there is none, and we
 * bind the framebuffer of the view (gl2.c). */
#if defined(HAVE_WGL_DXGI)
#include "../drivers_context/wgl_dxgi.h"
#define gl_window_framebuffer() wgl_window_framebuffer()
/* The buffer in it from which we read a screenshot. */
#define gl_window_read_buffer() (wgl_window_framebuffer() ? GL_COLOR_ATTACHMENT0 : GL_BACK)
#else
#define gl_window_framebuffer() 0
#define gl_window_read_buffer() GL_BACK
#endif

/* Whether we set the viewport again before we draw each frame. On Apple
 * drawables we lose it, on iOS every frame and in a Cocoa view after a live
 * resize, which may come without another resize notification. On other
 * platforms the viewport stays until the window changes. */
#if defined(IOS) || defined(OSX)
#define GL_WINDOW_LOSES_VIEWPORT 1
#else
#define GL_WINDOW_LOSES_VIEWPORT 0
#endif

#endif
