#include "document_contract.hpp"
#include "declarations.h"
#include "document.hpp"
#include "../../../verbosity.h"
#include "elements.hpp"
#include <RmlUi/Core/StringUtilities.h>
#include <streams/file_stream.h>
#include "render/rmlui_gl.h"
#include <filesystem>
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>

#ifndef RIB_RMLUI_HEADLESS
#include "render/platform.h"
#include "../third_party/lodepng.h"
#endif

namespace rib
{
namespace
{
#ifdef RIB_RMLUI_HEADLESS
class HeadlessRenderer : public RominaboxRenderer
{
public:
   HeadlessRenderer(unsigned &texture_count, unsigned &geometry_count)
      : texture_count(texture_count), geometry_count(geometry_count) {}
   void SetViewport(int, int) override {}
   void BeginFrame() override {}
   void EndFrame() override {}

   Rml::CompiledGeometryHandle CompileGeometry(
         Rml::Span<const Rml::Vertex>, Rml::Span<const int>) override
   {
      return ++geometry_count;
   }
   void RenderGeometry(Rml::CompiledGeometryHandle, Rml::Vector2f,
         Rml::TextureHandle) override {}
   void ReleaseGeometry(Rml::CompiledGeometryHandle) override {}
   Rml::TextureHandle LoadTexture(Rml::Vector2i& dimensions,
         const Rml::String&) override
   {
      ++texture_count;
      dimensions = {1, 1};
      return 1;
   }
   Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte>,
         Rml::Vector2i) override
   {
      return 1;
   }
   void ReleaseTexture(Rml::TextureHandle) override {}
   void EnableScissorRegion(bool) override {}
   void SetScissorRegion(Rml::Rectanglei) override {}

private:
   unsigned &texture_count;
   /* The count of geometry pieces built in RmlUi. After a change, only what
    * must be laid out or drawn again is built again. */
   unsigned &geometry_count;
};

#endif

}

Document::Document() = default;
Document::~Document() { shutdown(); }

void Document::System::JoinPath(Rml::String& output,
      const Rml::String& document_path, const Rml::String& path)
{
   // These are filesystem resources, not web-root-relative URLs.
   const auto child = std::filesystem::u8path(path);
   const auto base = std::filesystem::u8path(document_path).parent_path();
   output = (child.is_absolute() ? child : base / child).lexically_normal().u8string();
}

Rml::FileHandle Document::FileLayer::Open(const Rml::String& path)
{
   return reinterpret_cast<Rml::FileHandle>(filestream_open(path.c_str(),
         RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE));
}

void Document::FileLayer::Close(Rml::FileHandle file)
{
   filestream_close(reinterpret_cast<RFILE*>(file));
}

size_t Document::FileLayer::Read(void *buffer, size_t size, Rml::FileHandle file)
{
   const int64_t read = filestream_read(reinterpret_cast<RFILE*>(file), buffer, (int64_t)size);
   return read > 0 ? (size_t)read : 0;
}

bool Document::FileLayer::Seek(Rml::FileHandle file, long offset, int origin)
{
   const int position = origin == SEEK_CUR ? RETRO_VFS_SEEK_POSITION_CURRENT
         : origin == SEEK_END ? RETRO_VFS_SEEK_POSITION_END
         : RETRO_VFS_SEEK_POSITION_START;
   return filestream_seek(reinterpret_cast<RFILE*>(file), offset, position) >= 0;
}

size_t Document::FileLayer::Tell(Rml::FileHandle file)
{
   const int64_t at = filestream_tell(reinterpret_cast<RFILE*>(file));
   return at > 0 ? (size_t)at : 0;
}

size_t Document::FileLayer::Length(Rml::FileHandle file)
{
   const int64_t size = filestream_get_size(reinterpret_cast<RFILE*>(file));
   return size > 0 ? (size_t)size : 0;
}

#ifdef RIB_RMLUI_HEADLESS
double Document::System::GetElapsedTime()
{
   return Rml::SystemInterface::GetElapsedTime() + clock_offset;
}
#endif

std::string Document::asset_path(const char *name) const
{
   return asset_dir + "/" + name;
}

bool Document::initialize(const char *asset_directory,
      const std::vector<std::string>& fonts, int width, int height, bool core_context)
{
   if (context)
      return true;
   if (!asset_directory || !*asset_directory)
      return false;

   asset_dir = asset_directory;
#ifdef RIB_RMLUI_HEADLESS
   (void)core_context;
   renderer = std::make_unique<HeadlessRenderer>(texture_count, geometry_count);
#else
   renderer = rib_menu_renderer(core_context);
#endif
   renderer->SetViewport(width, height);
   Rml::SetSystemInterface(&system);
   Rml::SetFileInterface(&file_layer);
   Rml::SetRenderInterface(renderer.get());

   if (!Rml::Initialise())
      return false;

   /* The fonts in the design. We also use the first one for any glyph missing
    * from a face. With a design that declares no font, we cannot draw text. */
   bool drawn = !fonts.empty();
   for (size_t index = 0; drawn && index < fonts.size(); ++index)
      drawn = Rml::LoadFontFace(asset_path(fonts[index].c_str()), false)
            && (index || Rml::LoadFontFace(asset_path(fonts[index].c_str()), true));
   if (!drawn)
   {
      Rml::Shutdown();
      renderer.reset();
      return false;
   }

#ifdef HAVE_COCOA
   text_input = make_text_input_platform(*this);
   system.text_input = text_input.get();
#endif
   context = Rml::CreateContext("rominabox-menu", Rml::Vector2i(width, height), nullptr, text_input.get());
   document = context ? context->LoadDocument(asset_path(files::Menu)) : nullptr;
   if (!context || !document)
   {
      Rml::Shutdown();
      context = nullptr;
      document = nullptr;
      renderer.reset();
      system.text_input = nullptr;
      text_input.reset();
      return false;
   }
   return true;
}

void Document::shutdown()
{
   if (context)
      Rml::RemoveContext("rominabox-menu");
   context = nullptr;
   document = nullptr;
   if (renderer)
      Rml::Shutdown();
   renderer.reset();
   system.text_input = nullptr;
   text_input.reset();
}

void Document::show()
{
   document->Show();
}

void Document::settle()
{
   context->SetDensityIndependentPixelRatio(1.0f);
   context->Update();
}

/* Read the frame we have just drawn for the menu, and write it.
 *
 * We do not use the RetroArch screenshot, whose code is in the runloop after
 * the buffer is presented. A viewport read at that point returns an empty
 * buffer, so the result is a black picture written without an error. Here
 * the frame of the core and the menu over it are still in the back buffer.
 *
 * GL returns the rows bottom-up, so we flip them. We drop alpha, because we
 * draw the menu over the game and a screenshot of it is opaque.
 */
void Document::write_capture(int width, int height)
{
   if (capture_path.empty() || width <= 0 || height <= 0)
      return;
#ifdef RIB_RMLUI_HEADLESS
   /* There is no renderer to read from in the headless build, and in the
    * bridge tests we send events and check no pixels. */
   capture_path.clear();
#else

   const std::string path = capture_path;
   capture_path.clear();
   std::vector<unsigned char> pixels((size_t)width * (size_t)height * 4);
   glPixelStorei(GL_PACK_ALIGNMENT, 1);
   glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

   std::vector<unsigned char> flipped(pixels.size());
   const size_t stride = (size_t)width * 4;
   for (int row = 0; row < height; ++row)
      std::memcpy(&flipped[(size_t)row * stride],
            &pixels[(size_t)(height - 1 - row) * stride], stride);
   for (size_t i = 3; i < flipped.size(); i += 4)
      flipped[i] = 255;

   const unsigned error = lodepng::encode(path, flipped,
         (unsigned)width, (unsigned)height);
   if (error)
      RARCH_ERR("[RIB] could not write %s: %s\n",
            path.c_str(), lodepng_error_text(error));
#endif
}

void Document::release_texture(const std::string& path)
{
   if (!renderer)
      return;
   /* In RmlUi a file texture is stored under the normalised path from
    * JoinPath. On Windows a slash in the data folder path becomes a
    * backslash, so we release the texture by that path, not the written one. */
   Rml::String source;
   system.JoinPath(source, "", path);
   Rml::ReleaseTexture(source, renderer.get());
}

void Document::capture_next(const char *path)
{
   capture_path = (path && *path) ? path : "";
}

void Document::render(int width, int height)
{
   if (!context || !renderer)
      return;
   context->SetDimensions(Rml::Vector2i(width, height));
   /* The design's canvas, scaled to fit the window whole. */
   const float density = std::min(
         static_cast<float>(width) / document_contract::kCanvasWidth,
         static_cast<float>(height) / document_contract::kCanvasHeight);
   context->SetDensityIndependentPixelRatio(std::max(density, 0.1f));
   renderer->SetViewport(width, height);
   context->Update();
   renderer->BeginFrame();
   context->Render();
   renderer->EndFrame();
   /* After the menu is drawn and before the frame is presented. */
   write_capture(width, height);
}

}

namespace rib {
bool Document::click_element(const char *id)
{
   if (!root() || !id || !*id)
      return false;
   Rml::Element *element = root()->GetElementById(id);
   if (!element)
      return false;
   element->Click();
   return true;
}

bool Document::element_center(const char *id, int *x, int *y)
{
   if (!get_context() || !root() || !id || !x || !y)
      return false;
   get_context()->Update();
   Rml::Element *element = root()->GetElementById(id);
   if (!element)
      return false;
   const Rml::Vector2f offset = element->GetAbsoluteOffset(Rml::BoxArea::Border);
   const Rml::Vector2f size = element->GetBox().GetSize(Rml::BoxArea::Border);
   *x = static_cast<int>(offset.x + size.x * 0.5f);
   *y = static_cast<int>(offset.y + size.y * 0.5f);
   return size.x > 0.f && size.y > 0.f;
}

bool Document::element_box(const char *id, int *x, int *y, int *w, int *h)
{
   if (!get_context() || !root() || !id || !x || !y || !w || !h)
      return false;
   get_context()->Update();
   Rml::Element *element = root()->GetElementById(id);
   if (!element)
      return false;
   const Rml::Vector2f offset = element->GetAbsoluteOffset(Rml::BoxArea::Border);
   const Rml::Vector2f size = element->GetBox().GetSize(Rml::BoxArea::Border);
   *x = static_cast<int>(offset.x);
   *y = static_cast<int>(offset.y);
   *w = static_cast<int>(size.x);
   *h = static_cast<int>(size.y);
   return size.x > 0.f && size.y > 0.f;
}

void Document::set_shown(const char *id, bool shown)
{
   if (root() && id)
      rib::show(root()->GetElementById(id), shown);
}

void Document::set_disabled(const char *id, bool disabled)
{
   if (root() && id)
      disable(root()->GetElementById(id), disabled);
}

bool Document::pointer_inside(const char *id, int x, int y)
{
   if (!root() || !get_context() || !id)
      return false;
   Rml::Element *element = root()->GetElementById(id);
   if (!element || hidden(element))
      return false;
   get_context()->Update();
   const Rml::Vector2f offset = element->GetAbsoluteOffset(Rml::BoxArea::Border);
   const Rml::Vector2f size = element->GetBox().GetSize(Rml::BoxArea::Border);
   return x >= (int)offset.x && x < (int)(offset.x + size.x)
         && y >= (int)offset.y && y < (int)(offset.y + size.y);
}

bool Document::has_element(const char *id)
{
   return root() && id && root()->GetElementById(id);
}

void Document::set_element_text(const char *id, const char *text)
{
   if (root() && id && *id)
      write_text(root()->GetElementById(id), text ? text : "");
}

void Document::show_fact(const char *fact, const std::string& text)
{
   if (!root() || !fact || !*fact) return;
   Rml::ElementList showing;
   root()->QuerySelectorAll(showing,
         std::string("[") + document_contract::FactAttribute + "=" + fact + "]");
   for (Rml::Element *element : showing)
      write_text(element, text);
}

void Document::show_game_shape(float aspect)
{
   const float dp = context ? context->GetDensityIndependentPixelRatio() : 0.0f;
   if (!root() || !(dp > 0.0f) || !(aspect > 0.0f && aspect < 100.0f)) return;
   /* In whole thousandths, which look the same in every locale. */
   const long thousandths = std::lround(aspect * 1000.0f);
   char shape[32];
   std::snprintf(shape, sizeof(shape), "%ld.%03ld", thousandths / 1000, thousandths % 1000);
   if (root()->GetAttribute<Rml::String>(document_contract::GameShapeAttribute, "") == shape)
      return;
   root()->SetAttribute(document_contract::GameShapeAttribute, Rml::String(shape));
   /* The largest sizes in the stylesheet now, which a design may also make
    * depend on the game's proportions. We set width and height, never these,
    * so they stay as the design set them however often the menu opens. */
   context->Update();
   Rml::ElementList marked;
   root()->QuerySelectorAll(marked,
         std::string("[") + document_contract::GameShapedAttribute + "]");
   for (Rml::Element *element : marked)
   {
      const Rml::Style::ComputedValues& style = element->GetComputedValues();
      const Rml::Style::LengthPercentage most_wide = style.max_width();
      const Rml::Style::LengthPercentage most_tall = style.max_height();
      const auto declared = [](const Rml::Style::LengthPercentage& most) {
         return most.type == Rml::Style::LengthPercentage::Length
               && most.value > 0.0f && most.value < FLT_MAX;
      };
      if (!declared(most_wide) || !declared(most_tall))
         continue;
      const float width = most_wide.value / dp, height = most_tall.value / dp;
      element->SetProperty(Rml::PropertyId::Width,
            Rml::Property(std::min(width, height * aspect), Rml::Unit::DP));
      element->SetProperty(Rml::PropertyId::Height,
            Rml::Property(std::min(height, width / aspect), Rml::Unit::DP));
   }
}

void Document::set_class(const char *id, const char *name, bool enabled)
{
   if (!root() || !id || !*id) return;
   if (auto *element = root()->GetElementById(id)) element->SetClass(name, enabled);
}
}

namespace rib {
void Document::System::ActivateKeyboard(Rml::Vector2f position, float line_height)
{
   if (text_input) text_input->caret(position, line_height);
}
void Document::System::GetClipboardText(Rml::String& text)
{
#ifdef RIB_RMLUI_HEADLESS
   text = test_clipboard;
#else
   if (text_input) text_input->get_clipboard(text);
   else Rml::SystemInterface::GetClipboardText(text);
#endif
}
void Document::System::SetClipboardText(const Rml::String& text)
{
#ifdef RIB_RMLUI_HEADLESS
   test_clipboard = text;
#else
   if (text_input) text_input->set_clipboard(text);
   else Rml::SystemInterface::SetClipboardText(text);
#endif
}
}
