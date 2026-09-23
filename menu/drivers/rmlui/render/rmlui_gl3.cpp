/* The RmlUi GL3 backend. We compile it alone so that it can include the core
 * profile header. The other file of the bridge includes gl.h for the legacy
 * backend, and the two headers cannot be in one translation unit. */
#define RIB_MENU_GL_CORE
#define RMLUI_GL3_CUSTOM_LOADER "rmlui/render/platform.h"
#include <RmlUi_Renderer_GL3.cpp>
