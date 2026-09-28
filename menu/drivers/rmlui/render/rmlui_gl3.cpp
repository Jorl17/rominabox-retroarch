/* The RmlUi GL3 backend. We compile it alone so that it can include the core
 * profile header. The other file of the bridge includes gl.h for the legacy
 * backend, and the two headers cannot be in one translation unit. */
#define RIB_MENU_GL_CORE
#define RMLUI_GL3_CUSTOM_LOADER "rmlui/render/platform.h"
#include "platform.h"
#include "rmlui_gl.h"

/* In the RmlUi GL3 backend, framebuffer 0 is the window, where the menu is
 * drawn at the end of each frame. Our window is the framebuffer bound when
 * the frame of the menu began, which is the DXGI one in a fullscreen Windows
 * game (gfx/common/gl_window.h), so in the backend 0 means that one. We
 * define this before the redirect, so that it calls the GL function. */
namespace
{
GLuint window_framebuffer = 0;

void bind_framebuffer(GLenum target, GLuint framebuffer)
{
   glBindFramebuffer(target, framebuffer ? framebuffer : window_framebuffer);
}
}

void rib_menu_gl3_window(unsigned framebuffer)
{
   window_framebuffer = framebuffer;
}

#undef glBindFramebuffer
#define glBindFramebuffer bind_framebuffer
#include <RmlUi_Renderer_GL3.cpp>
