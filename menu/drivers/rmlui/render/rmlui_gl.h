#pragma once

/* The menu renderer, with GL2 for a legacy context and GL3 for a core
 * profile. Choose one once, from the flag that we set in the gl driver when
 * we create the context. */

#include <RmlUi/Core/RenderInterface.h>

#include <memory>

class RominaboxRenderer : public Rml::RenderInterface
{
public:
   virtual void SetViewport(int width, int height) = 0;
   virtual void BeginFrame() = 0;
   virtual void EndFrame() = 0;
};

std::unique_ptr<RominaboxRenderer> rib_menu_renderer(bool core_context);

/* The framebuffer for the finished menu in the GL3 backend, which is the one
 * bound for the window when the frame of the menu began. We set it in the
 * renderer at the start of each frame, and keep it in rmlui_gl3.cpp. */
void rib_menu_gl3_window(unsigned framebuffer);
