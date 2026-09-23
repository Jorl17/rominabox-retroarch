#include "document_contract.hpp"
#include "document.hpp"
#include "elements.hpp"
#include <RmlUi/Core/StringUtilities.h>
#include "render/rmlui_gl.h"
#include <RmlUi/Core/Factory.h>
#include <filesystem>
#include <sys/stat.h>
#include <algorithm>
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
   explicit HeadlessRenderer(unsigned &texture_count) : texture_count(texture_count) {}
   void SetViewport(int, int) override {}
   void BeginFrame() override {}
   void EndFrame() override {}

   Rml::CompiledGeometryHandle CompileGeometry(
         Rml::Span<const Rml::Vertex>, Rml::Span<const int>) override
   {
      return ++geometry;
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
   Rml::CompiledGeometryHandle geometry = 0;
};

#endif

time_t modification_time(const std::string& path)
{
   struct stat info = {};
   return stat(path.c_str(), &info) == 0 ? info.st_mtime : 0;
}

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

bool Document::initialize(
      const char *asset_directory, int width, int height, bool core_context)
{
   if (context)
      return true;
   if (!asset_directory || !*asset_directory)
      return false;

   asset_dir = asset_directory;
#ifdef RIB_RMLUI_HEADLESS
   (void)core_context;
   renderer = std::make_unique<HeadlessRenderer>(texture_count);
#else
   renderer = rib_menu_renderer(core_context);
#endif
   renderer->SetViewport(width, height);
   Rml::SetSystemInterface(&system);
   Rml::SetRenderInterface(renderer.get());

   if (!Rml::Initialise())
      return false;

   if (!Rml::LoadFontFace(asset_path("Silkscreen-Regular.ttf"), false) ||
       !Rml::LoadFontFace(asset_path("Silkscreen-Regular.ttf"), true))
   {
      Rml::Shutdown();
      renderer.reset();
      return false;
   }

   context = Rml::CreateContext("rominabox-menu", Rml::Vector2i(width, height));
   document = context ? context->LoadDocument(asset_path("menu.rml")) : nullptr;
   if (!context || !document)
   {
      Rml::Shutdown();
      context = nullptr;
      document = nullptr;
      renderer.reset();
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
}

void Document::show()
{
   document->Show();
   rml_mtime = modification_time(asset_path("menu.rml"));
   rcss_mtime = modification_time(asset_path("menu.rcss"));
}

void Document::settle()
{
   context->SetDensityIndependentPixelRatio(1.0f);
   context->Update();
}

bool Document::reload_if_changed(void)
{
   if (!context || !document)
      return false;

   const time_t current_rml_mtime = modification_time(asset_path("menu.rml"));
   const time_t current_rcss_mtime = modification_time(asset_path("menu.rcss"));
   if (current_rml_mtime == rml_mtime && current_rcss_mtime == rcss_mtime)
      return false;

   document->Close();
   document = nullptr;
   Rml::Factory::ClearStyleSheetCache();
   document = context->LoadDocument(asset_path("menu.rml"));
   return document != nullptr;
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
      std::fprintf(stderr, "[RIB] could not write %s: %s\n",
            path.c_str(), lodepng_error_text(error));
#endif
}

void Document::release_texture(const std::string& path)
{
   if (renderer)
      Rml::ReleaseTexture(path, renderer.get());
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
   const float density = std::min(
         static_cast<float>(width) / 960.0f,
         static_cast<float>(height) / 600.0f);
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

int Document::focusables(const char *panel, char ids[][64], int capacity)
{
   return rib::focusable_ids(root(), panel, ids, capacity);
}

void Document::mark_focused(const char *panel, const char *id)
{
   char ids[16][64];
   const int count = focusables(panel, ids, 16);
   for (int index = 0; index < count; ++index)
      if (Rml::Element *element = root()->GetElementById(ids[index]))
         element->SetClass(document_contract::Focused, id && std::strcmp(ids[index], id) == 0);
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

bool Document::element_disabled(const char *id)
{
   if (!root() || !id)
      return false;
   Rml::Element *element = root()->GetElementById(id);
   return element && element->HasAttribute("disabled");
}

void Document::set_shown(const char *id, bool shown)
{
   if (!root() || !id)
      return;
   if (Rml::Element *element = root()->GetElementById(id))
   {
      if (shown)
         element->RemoveProperty("display");
      else
         element->SetProperty("display", "none");
   }
}

void Document::set_disabled(const char *id, bool disabled)
{
   if (!root() || !id)
      return;
   Rml::Element *element = root()->GetElementById(id);
   if (!element)
      return;
   element->SetClass(document_contract::Disabled, disabled);
   if (disabled)
      element->SetAttribute("disabled", "disabled");
   else
      element->RemoveAttribute("disabled");
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
   if (!root() || !id || !*id) return;
   if (auto *element = root()->GetElementById(id))
      element->SetInnerRML(Rml::StringUtilities::EncodeRml(text ? text : ""));
}

void Document::set_class(const char *id, const char *name, bool enabled)
{
   if (!root() || !id || !*id) return;
   if (auto *element = root()->GetElementById(id)) element->SetClass(name, enabled);
}
}
