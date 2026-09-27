#pragma once

/* The GL entry points that we call in the menu renderer. Keep the legacy and
 * core headers in separate translation units. In the renderers we restore the
 * RetroArch GL state around RmlUi. We name a loader here for each platform
 * we build the player for. */
#if defined(__APPLE__)
#ifdef RIB_MENU_GL_CORE
#include <OpenGL/gl3.h>
#else
#include <OpenGL/gl.h>
#ifndef GL_VERTEX_ARRAY_BINDING
#define GL_VERTEX_ARRAY_BINDING 0x85B5
#endif
extern "C" void glBindVertexArray(GLuint array);
#endif
#elif defined(_WIN32)
/* opengl32.dll has only OpenGL 1.1. The rest is declared in the RetroArch
 * loader and resolved in the gl driver before we draw any menu. */
#include <glsym/glsym.h>
#else
#error "the menu renderer has no GL entry points declared for this platform"
#endif
