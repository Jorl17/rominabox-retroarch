#include "rmlui_gl.h"
#include "platform.h"

#include <RmlUi_Renderer_GL2.h>
#include <RmlUi_Renderer_GL2.cpp>
#include <RmlUi_Renderer_GL3.h>
#include "../../third_party/lodepng.h"
#include <streams/file_stream.h>

#include <cstdio>
#include <cstdlib>
#include <type_traits>
#include <vector>

namespace
{
/* Restore what we already restored in the GL2 path, with the fixed-function
 * calls of a compatibility context. Each of them is an error in a core
 * profile, so we do not use this there. */
struct LegacyGlState
{
   GLint program = 0;
   GLint array_buffer = 0;
   GLint element_buffer = 0;
   GLint matrix_mode = GL_MODELVIEW;

   void save()
   {
      glGetIntegerv(GL_CURRENT_PROGRAM, &program);
      glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &array_buffer);
      glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &element_buffer);
      glGetIntegerv(GL_MATRIX_MODE, &matrix_mode);
      glPushAttrib(GL_ALL_ATTRIB_BITS);
      glPushClientAttrib(GL_CLIENT_ALL_ATTRIB_BITS);
      /* After the RetroArch GLSL path, generic vertex attribute arrays may
       * still be enabled. Where attribute 0 is also the fixed-function vertex
       * position (in the NVIDIA compatibility profile), an enabled array
       * replaces the menu positions and nothing appears. We restore their
       * enable bits with the client vertex-array state in the pop below. */
      GLint attributes = 0;
      glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &attributes);
      for (GLint i = 0; i < attributes; i++)
         glDisableVertexAttribArray((GLuint)i);
      glMatrixMode(GL_PROJECTION);
      glPushMatrix();
      glMatrixMode(GL_MODELVIEW);
      glPushMatrix();
      glMatrixMode(GL_TEXTURE);
      glPushMatrix();
      glUseProgram(0);
      glBindBuffer(GL_ARRAY_BUFFER, 0);
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
   }

   void restore() const
   {
      glMatrixMode(GL_TEXTURE);
      glPopMatrix();
      glMatrixMode(GL_MODELVIEW);
      glPopMatrix();
      glMatrixMode(GL_PROJECTION);
      glPopMatrix();
      glMatrixMode(matrix_mode);
      glBindBuffer(GL_ARRAY_BUFFER, array_buffer);
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, element_buffer);
      glUseProgram(program);
      glPopClientAttrib();
      glPopAttrib();
   }
};

/* A core profile context, such as the OpenGL 4.1 core context of Flycast,
 * has no glPushAttrib and no matrix stack, so with the save above we cannot
 * draw the menu or restore the GL state of the core there. These are the
 * core profile calls for the state that we change for the menu, including
 * the framebuffer, because in the GL3 backend we bind the default
 * framebuffer in EndFrame and do not restore the previous one. */
struct CoreGlState
{
   GLint program = 0;
   GLint vao = 0;
   GLint array_buffer = 0;
   GLint element_buffer = 0;
   GLint active_texture = GL_TEXTURE0;
   GLint texture_binding = 0;
   GLint texture0_binding = 0;
   GLint framebuffer = 0;
   GLboolean blend = GL_FALSE;
   GLboolean scissor = GL_FALSE;
   GLboolean depth = GL_FALSE;
   GLboolean stencil = GL_FALSE;
   GLboolean cull = GL_FALSE;
   GLint blend_src_rgb = GL_ONE;
   GLint blend_dst_rgb = GL_ZERO;
   GLint blend_src_alpha = GL_ONE;
   GLint blend_dst_alpha = GL_ZERO;
   GLint blend_eq_rgb = GL_FUNC_ADD;
   GLint blend_eq_alpha = GL_FUNC_ADD;
   GLint scissor_box[4] = {};
   GLint viewport[4] = {};

   void save()
   {
      glGetIntegerv(GL_CURRENT_PROGRAM, &program);
      glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
      glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &array_buffer);
      glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &element_buffer);
      glGetIntegerv(GL_ACTIVE_TEXTURE, &active_texture);
      glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture_binding);
      glActiveTexture(GL_TEXTURE0);
      glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture0_binding);
      glActiveTexture(active_texture);
      glGetIntegerv(GL_FRAMEBUFFER_BINDING, &framebuffer);
      blend = glIsEnabled(GL_BLEND);
      scissor = glIsEnabled(GL_SCISSOR_TEST);
      depth = glIsEnabled(GL_DEPTH_TEST);
      stencil = glIsEnabled(GL_STENCIL_TEST);
      cull = glIsEnabled(GL_CULL_FACE);
      glGetIntegerv(GL_BLEND_SRC_RGB, &blend_src_rgb);
      glGetIntegerv(GL_BLEND_DST_RGB, &blend_dst_rgb);
      glGetIntegerv(GL_BLEND_SRC_ALPHA, &blend_src_alpha);
      glGetIntegerv(GL_BLEND_DST_ALPHA, &blend_dst_alpha);
      glGetIntegerv(GL_BLEND_EQUATION_RGB, &blend_eq_rgb);
      glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &blend_eq_alpha);
      glGetIntegerv(GL_SCISSOR_BOX, scissor_box);
      glGetIntegerv(GL_VIEWPORT, viewport);
   }

   void restore() const
   {
      glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
      glUseProgram(program);
      glBindVertexArray(vao);
      glBindBuffer(GL_ARRAY_BUFFER, array_buffer);
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, element_buffer);
      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D, texture0_binding);
      glActiveTexture(active_texture);
      glBindTexture(GL_TEXTURE_2D, texture_binding);
      set_cap(GL_BLEND, blend);
      set_cap(GL_SCISSOR_TEST, scissor);
      set_cap(GL_DEPTH_TEST, depth);
      set_cap(GL_STENCIL_TEST, stencil);
      set_cap(GL_CULL_FACE, cull);
      glBlendFuncSeparate(blend_src_rgb, blend_dst_rgb, blend_src_alpha, blend_dst_alpha);
      glBlendEquationSeparate(blend_eq_rgb, blend_eq_alpha);
      glScissor(scissor_box[0], scissor_box[1], scissor_box[2], scissor_box[3]);
      glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
   }

   static void set_cap(GLenum cap, GLboolean on)
   {
      if (on)
         glEnable(cap);
      else
         glDisable(cap);
   }
};

/* The unpack state for the pictures of the menu, tightly packed, from the
 * pointer from RmlUi. For the frame of a game we set these for its upload,
 * and after glcore the row length is still the pitch of the frame. With that
 * stride we would read every glyph wrong and garble the text of the menu.
 * In RmlUi the pictures are made while we draw the menu, so we set these to
 * the GL defaults for the frame of the menu and restore them after it, in
 * either context. */
struct UnpackState
{
   GLint buffer = 0;
   GLint row_length = 0;
   GLint skip_rows = 0;
   GLint skip_pixels = 0;
   GLint alignment = 4;

   void save()
   {
      glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &buffer);
      glGetIntegerv(GL_UNPACK_ROW_LENGTH, &row_length);
      glGetIntegerv(GL_UNPACK_SKIP_ROWS, &skip_rows);
      glGetIntegerv(GL_UNPACK_SKIP_PIXELS, &skip_pixels);
      glGetIntegerv(GL_UNPACK_ALIGNMENT, &alignment);
      glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
      glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
      glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
      glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
      glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
   }

   void restore() const
   {
      glBindBuffer(GL_PIXEL_UNPACK_BUFFER, buffer);
      glPixelStorei(GL_UNPACK_ROW_LENGTH, row_length);
      glPixelStorei(GL_UNPACK_SKIP_ROWS, skip_rows);
      glPixelStorei(GL_UNPACK_SKIP_PIXELS, skip_pixels);
      glPixelStorei(GL_UNPACK_ALIGNMENT, alignment);
   }
};

Rml::TextureHandle load_png(Rml::RenderInterface& backend,
      Rml::Vector2i& dimensions, const Rml::String& source)
{
   /* We read through the libretro file layer, with UTF-8 paths on every
    * platform. The fopen in lodepng takes ANSI code page paths on Windows, so
    * with it we could not open a slot picture under a non-ASCII folder name. */
   void *file = nullptr;
   int64_t size = 0;
   if (!filestream_read_file(source.c_str(), &file, &size))
   {
      Rml::Log::Message(Rml::Log::LT_ERROR,
            "Could not read PNG texture %s", source.c_str());
      return {};
   }
   std::vector<unsigned char> rgba;
   unsigned width = 0;
   unsigned height = 0;
   const unsigned error = lodepng::decode(rgba, width, height,
         static_cast<const unsigned char*>(file), static_cast<size_t>(size));
   free(file);
   if (error)
   {
      Rml::Log::Message(Rml::Log::LT_ERROR,
            "Could not decode PNG texture %s: %s",
            source.c_str(), lodepng_error_text(error));
      return {};
   }

   dimensions = {static_cast<int>(width), static_cast<int>(height)};
   return backend.GenerateTexture(
         Rml::Span<const Rml::byte>(rgba.data(), rgba.size()), dimensions);
}

template <typename Backend>
class RominaboxGl final : public RominaboxRenderer
{
public:
   void SetViewport(int width, int height) override
   {
      backend.SetViewport(width, height);
   }

   void BeginFrame() override
   {
      if constexpr (std::is_same<Backend, RenderInterface_GL3>::value)
      {
         core.save();
         rib_menu_gl3_window((unsigned)core.framebuffer);
      }
      else
         legacy.save();
      unpack.save();
      backend.BeginFrame();
   }

   void EndFrame() override
   {
      backend.EndFrame();
      unpack.restore();
      if constexpr (std::is_same<Backend, RenderInterface_GL3>::value)
         core.restore();
      else
         legacy.restore();
   }

   Rml::CompiledGeometryHandle CompileGeometry(
         Rml::Span<const Rml::Vertex> vertices, Rml::Span<const int> indices) override
   {
      return backend.CompileGeometry(vertices, indices);
   }

   void RenderGeometry(Rml::CompiledGeometryHandle geometry,
         Rml::Vector2f translation, Rml::TextureHandle texture) override
   {
      backend.RenderGeometry(geometry, translation, texture);
   }

   void ReleaseGeometry(Rml::CompiledGeometryHandle geometry) override
   {
      backend.ReleaseGeometry(geometry);
   }

   Rml::TextureHandle LoadTexture(Rml::Vector2i& dimensions,
         const Rml::String& source) override
   {
      return load_png(backend, dimensions, source);
   }

   Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte> source,
         Rml::Vector2i dimensions) override
   {
      return backend.GenerateTexture(source, dimensions);
   }

   void ReleaseTexture(Rml::TextureHandle texture) override
   {
      backend.ReleaseTexture(texture);
   }

   void EnableScissorRegion(bool enable) override
   {
      backend.EnableScissorRegion(enable);
   }

   void SetScissorRegion(Rml::Rectanglei region) override
   {
      backend.SetScissorRegion(region);
   }

   void EnableClipMask(bool enable) override
   {
      backend.EnableClipMask(enable);
   }

   void RenderToClipMask(Rml::ClipMaskOperation operation,
         Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation) override
   {
      backend.RenderToClipMask(operation, geometry, translation);
   }

   void SetTransform(const Rml::Matrix4f* transform) override
   {
      backend.SetTransform(transform);
   }

   Rml::LayerHandle PushLayer() override { return backend.PushLayer(); }

   void CompositeLayers(Rml::LayerHandle source, Rml::LayerHandle destination,
         Rml::BlendMode blend_mode,
         Rml::Span<const Rml::CompiledFilterHandle> filters) override
   {
      backend.CompositeLayers(source, destination, blend_mode, filters);
   }

   void PopLayer() override { backend.PopLayer(); }

   Rml::TextureHandle SaveLayerAsTexture() override
   {
      return backend.SaveLayerAsTexture();
   }

   Rml::CompiledFilterHandle SaveLayerAsMaskImage() override
   {
      return backend.SaveLayerAsMaskImage();
   }

   Rml::CompiledFilterHandle CompileFilter(const Rml::String& name,
         const Rml::Dictionary& parameters) override
   {
      return backend.CompileFilter(name, parameters);
   }

   void ReleaseFilter(Rml::CompiledFilterHandle filter) override
   {
      backend.ReleaseFilter(filter);
   }

   Rml::CompiledShaderHandle CompileShader(const Rml::String& name,
         const Rml::Dictionary& parameters) override
   {
      return backend.CompileShader(name, parameters);
   }

   void RenderShader(Rml::CompiledShaderHandle shader,
         Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation,
         Rml::TextureHandle texture) override
   {
      backend.RenderShader(shader, geometry, translation, texture);
   }

   void ReleaseShader(Rml::CompiledShaderHandle shader) override
   {
      backend.ReleaseShader(shader);
   }

   bool shaders_ready() const
   {
      if constexpr (std::is_same<Backend, RenderInterface_GL3>::value)
         return static_cast<bool>(backend);
      return true;
   }

private:
   Backend backend;
   LegacyGlState legacy;
   CoreGlState core;
   UnpackState unpack;
};
}

std::unique_ptr<RominaboxRenderer> rib_menu_renderer(bool core_context)
{
   /* We record this in the gl driver when we create the context, from the hw
    * render type of the core, and read it once, at menu init. With GL2 we draw
    * with client arrays, which do nothing in a core profile, so we cannot use
    * GL2 in a core profile context, such as the one of a Dreamcast core. */
   if (core_context)
   {
      auto renderer = std::make_unique<RominaboxGl<RenderInterface_GL3>>();
      if (!renderer->shaders_ready())
         std::fprintf(stderr, "[RIB] core-profile menu shaders did not compile.\n");
      std::fprintf(stderr, "[RIB] menu renderer is GL3.\n");
      return renderer;
   }
   std::fprintf(stderr, "[RIB] menu renderer is GL2.\n");
   return std::make_unique<RominaboxGl<RenderInterface_GL2>>();
}
