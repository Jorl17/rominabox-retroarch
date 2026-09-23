#pragma once

#include <RmlUi/Core.h>
#include <RmlUi/Core/SystemInterface.h>
#include <ctime>
#include <memory>
#include <string>

class RominaboxRenderer;

namespace rib
{
/* The RmlUi runtime and its document. Feature state and listeners are in the
 * menu, and reload returns only after the new document has loaded. */
class Document
{
public:
   Document();
   ~Document();
   Document(const Document&) = delete;
   Document& operator=(const Document&) = delete;

   bool initialize(const char *assets, int width, int height, bool core_context);
   void shutdown();
   void show();
   void settle();
   bool reload_if_changed();
   void render(int width, int height);
   void capture_next(const char *path);
   void release_texture(const std::string& path);
   Rml::ElementDocument *root() const { return document; }
   Rml::Context *get_context() const { return context; }
   double elapsed() { return system.GetElapsedTime(); }
   std::string asset_path(const char *name) const;
#ifdef RIB_RMLUI_HEADLESS
   void advance(double seconds) { system.clock_offset += seconds; }
   unsigned texture_loads() const { return texture_count; }
#endif

private:
   struct System : Rml::SystemInterface
   {
      void JoinPath(Rml::String& output, const Rml::String& document_path,
            const Rml::String& path) override;
#ifdef RIB_RMLUI_HEADLESS
      double clock_offset = 0;
      double GetElapsedTime() override;
#endif
   } system;
   void write_capture(int width, int height);
   std::unique_ptr<RominaboxRenderer> renderer;
   Rml::Context *context = nullptr;
   Rml::ElementDocument *document = nullptr;
   std::string asset_dir;
   std::string capture_path;
   time_t rml_mtime = 0;
   time_t rcss_mtime = 0;
#ifdef RIB_RMLUI_HEADLESS
   unsigned texture_count = 0;
#endif
};
}
