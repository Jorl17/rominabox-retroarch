#pragma once

/* Layers for the RmlUi GL2 backend, as in the RmlUi GL3 backend. We include
 * this once, in rmlui_gl.cpp, after the backend and the legacy GL headers.
 *
 * In RmlUi, a box-shadow is drawn into a separate layer, which is kept as a
 * texture for drawing the element. Filters and mask images also go into
 * layers. The GL2 backend has no layers. Its PushLayer contains no bind, so
 * a layer goes into the window, and its SaveLayerAsTexture returns no texture.
 * Without this file, an element with a shadow would appear as a white block
 * without texture, with its shadow and background in the window over its
 * top-left corner.
 *
 * As in the GL3 backend, we draw a frame into a layer and composite it into
 * the window at the end. Each layer is a framebuffer with a colour texture
 * the size of the viewport. All layers share one stencil buffer, so the clip
 * mask from RmlUi applies in whichever layer we draw into. Layer 0 is the
 * layer of the frame, and the RmlUi layer handles are indices. We compile no
 * filter, so we composite a layer as we drew it. */

#include <vector>

class RenderInterface_GL2Layers : public RenderInterface_GL2
{
public:
   RenderInterface_GL2Layers() = default;
   RenderInterface_GL2Layers(const RenderInterface_GL2Layers&) = delete;
   RenderInterface_GL2Layers& operator=(const RenderInterface_GL2Layers&) = delete;

   ~RenderInterface_GL2Layers() { release(); }

   void SetViewport(int width, int height)
   {
      RenderInterface_GL2::SetViewport(width, height);
      viewport = {width, height};
   }

   /* The window is the framebuffer bound when the frame begins. */
   void BeginFrame()
   {
      glGetIntegerv(GL_FRAMEBUFFER_BINDING, &window);
      if (size != viewport)
      {
         release();
         size = viewport;
      }
      RenderInterface_GL2::BeginFrame();
      /* In RmlUi the scissor region is set where needed. We clear all of
       * layer 0, because we draw all of it in EndFrame. */
      glDisable(GL_SCISSOR_TEST);
      PushLayer();
   }

   /* Draw layer 0 over the window with the premultiplied blend that we use
    * for everything in the backend, and leave the window bound. */
   void EndFrame()
   {
      glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)window);
      glDisable(GL_SCISSOR_TEST);
      glDisable(GL_STENCIL_TEST);
      draw_layer(0, Rml::BlendMode::Blend);
      top = 0;
      RenderInterface_GL2::EndFrame();
   }

   Rml::LayerHandle PushLayer() override
   {
      if (top == layers.size())
         layers.push_back(make_layer());
      glBindFramebuffer(GL_FRAMEBUFFER, layers[top].framebuffer);
      glClearColor(0, 0, 0, 0);
      glClear(GL_COLOR_BUFFER_BIT);
      return (Rml::LayerHandle)top++;
   }

   void CompositeLayers(Rml::LayerHandle source, Rml::LayerHandle destination,
         Rml::BlendMode blend_mode, Rml::Span<const Rml::CompiledFilterHandle>) override
   {
      glBindFramebuffer(GL_FRAMEBUFFER, layers[destination].framebuffer);
      draw_layer(source, blend_mode);
      glBindFramebuffer(GL_FRAMEBUFFER, layers[top - 1].framebuffer);
   }

   void PopLayer() override
   {
      top--;
      glBindFramebuffer(GL_FRAMEBUFFER, layers[top - 1].framebuffer);
   }

   /* Copy the top layer inside the scissor region, or all of it without one,
    * into a texture whose first row is the top of the region, as in a texture
    * made from a picture. */
   Rml::TextureHandle SaveLayerAsTexture() override
   {
      GLint region[4] = {0, 0, size.x, size.y};
      const bool scissor = glIsEnabled(GL_SCISSOR_TEST);
      if (scissor)
         glGetIntegerv(GL_SCISSOR_BOX, region);
      const GLint width = region[2];
      const GLint height = region[3];
      GLuint texture = 0;
      glGenTextures(1, &texture);
      if (!texture)
         return {};
      glBindTexture(GL_TEXTURE_2D, texture);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
      set_texture_parameters();
      if (!copy)
         glGenFramebuffers(1, &copy);
      glBindFramebuffer(GL_READ_FRAMEBUFFER, layers[top - 1].framebuffer);
      glBindFramebuffer(GL_DRAW_FRAMEBUFFER, copy);
      glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
      glDisable(GL_SCISSOR_TEST);
      /* Upside down, because the first row of a framebuffer is its bottom. */
      glBlitFramebuffer(region[0], region[1], region[0] + width, region[1] + height,
            0, height, width, 0, GL_COLOR_BUFFER_BIT, GL_NEAREST);
      if (scissor)
         glEnable(GL_SCISSOR_TEST);
      glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
      glBindFramebuffer(GL_FRAMEBUFFER, layers[top - 1].framebuffer);
      return (Rml::TextureHandle)texture;
   }

private:
   struct Layer
   {
      GLuint framebuffer = 0;
      GLuint texture = 0;
   };

   static void set_texture_parameters()
   {
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
   }

   Layer make_layer()
   {
      Layer layer;
      glGenTextures(1, &layer.texture);
      glBindTexture(GL_TEXTURE_2D, layer.texture);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, size.x, size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
      set_texture_parameters();
      glGenFramebuffers(1, &layer.framebuffer);
      glBindFramebuffer(GL_FRAMEBUFFER, layer.framebuffer);
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, layer.texture, 0);
      GLint bound_renderbuffer = 0;
      glGetIntegerv(GL_RENDERBUFFER_BINDING, &bound_renderbuffer);
      if (!stencil)
      {
         glGenRenderbuffers(1, &stencil);
         glBindRenderbuffer(GL_RENDERBUFFER, stencil);
         glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, size.x, size.y);
      }
      glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, stencil);
      glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, stencil);
      glBindRenderbuffer(GL_RENDERBUFFER, (GLuint)bound_renderbuffer);
      if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
         Rml::Log::Message(Rml::Log::LT_ERROR, "The menu's layer framebuffer is incomplete.");
      return layer;
   }

   /* Draw the layer over the whole bound framebuffer, inside the current
    * scissor region and clip mask, whatever the current transform. */
   void draw_layer(Rml::LayerHandle layer, Rml::BlendMode blend_mode)
   {
      const float w = (float)size.x;
      const float h = (float)size.y;
      const Rml::ColourbPremultiplied white(255, 255, 255, 255);
      /* The first row of a layer is the bottom of the viewport. */
      const Rml::Vertex vertices[4] = {
         {{0, 0}, white, {0, 1}},
         {{w, 0}, white, {1, 1}},
         {{w, h}, white, {1, 0}},
         {{0, h}, white, {0, 0}},
      };
      const int indices[6] = {0, 1, 2, 0, 2, 3};
      const Rml::CompiledGeometryHandle quad = CompileGeometry(
            Rml::Span<const Rml::Vertex>(vertices, 4), Rml::Span<const int>(indices, 6));
      glPushMatrix();
      glLoadIdentity();
      if (blend_mode == Rml::BlendMode::Replace)
         glDisable(GL_BLEND);
      RenderGeometry(quad, {}, (Rml::TextureHandle)layers[layer].texture);
      if (blend_mode == Rml::BlendMode::Replace)
         glEnable(GL_BLEND);
      glPopMatrix();
      ReleaseGeometry(quad);
   }

   void release()
   {
      for (const Layer& layer : layers)
      {
         glDeleteFramebuffers(1, &layer.framebuffer);
         glDeleteTextures(1, &layer.texture);
      }
      layers.clear();
      if (stencil)
         glDeleteRenderbuffers(1, &stencil);
      stencil = 0;
      if (copy)
         glDeleteFramebuffers(1, &copy);
      copy = 0;
   }

   Rml::Vector2i viewport;
   Rml::Vector2i size;
   GLint window = 0;
   std::vector<Layer> layers;
   size_t top = 0;
   GLuint stencil = 0;
   GLuint copy = 0;
};
