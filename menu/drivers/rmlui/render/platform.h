#pragma once

/* GL entry points for the Mac only. Keep the legacy and core headers in
 * separate translation units. In the renderers we restore the RetroArch GL
 * state around RmlUi. Add a loader here for each other native platform. */
#ifdef RIB_MENU_GL_CORE
#include <OpenGL/gl3.h>
#else
#include <OpenGL/gl.h>
#ifndef GL_VERTEX_ARRAY_BINDING
#define GL_VERTEX_ARRAY_BINDING 0x85B5
#endif
extern "C" void glBindVertexArray(GLuint array);
#endif
