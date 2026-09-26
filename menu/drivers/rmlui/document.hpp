#pragma once

#include <RmlUi/Core.h>
#include <RmlUi/Core/SystemInterface.h>
#include <memory>
#include <string>
#include <vector>
#include "text_input_platform.hpp"

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

   /* The composed document in `assets`, drawn with `fonts`, the files
    * declared in the design next to it. */
   bool initialize(const char *assets, const std::vector<std::string>& fonts,
         int width, int height, bool core_context);
   void shutdown();
   void show();
   void settle();
   void render(int width, int height);
   void capture_next(const char *path);
   bool click_element(const char *id);
   bool element_center(const char *id, int *x, int *y);
   bool element_box(const char *id, int *x, int *y, int *w, int *h);
   bool pointer_inside(const char *id, int x, int y);
   bool has_element(const char *id);
   void set_element_text(const char *id, const char *text);
   /* Show `text` in every element that shows `fact` (data-fact). */
   void show_fact(const char *fact, const std::string& text);
   /* The proportions of the running game, width over height. We state them
    * on the document as data-game-shape and give them to every element
    * marked data-game-shaped, inside the largest size in its stylesheet.
    * No change for proportions already shown, or for none at all. */
   void show_game_shape(float aspect);
   void set_shown(const char *id, bool shown);
   void set_disabled(const char *id, bool disabled);
   void set_class(const char *id, const char *name, bool enabled);
   void release_texture(const std::string& path);
   Rml::ElementDocument *root() const { return document; }
   Rml::Context *get_context() const { return context; }
   double elapsed() { return system.GetElapsedTime(); }
   std::string asset_path(const char *name) const;
#ifdef RIB_RMLUI_HEADLESS
   void advance(double seconds) { system.clock_offset += seconds; }
   unsigned texture_loads() const { return texture_count; }
   /* How much geometry we have built. In a frame with no change we build
    * none. */
   unsigned geometry_compiled() const { return geometry_count; }
#endif

private:
   struct System : Rml::SystemInterface
   {
      TextInputPlatform *text_input = nullptr;
      void ActivateKeyboard(Rml::Vector2f position, float line_height) override;
      void GetClipboardText(Rml::String& text) override;
      void SetClipboardText(const Rml::String& text) override;
#ifdef RIB_RMLUI_HEADLESS
      std::string test_clipboard;
#endif
      void JoinPath(Rml::String& output, const Rml::String& document_path,
            const Rml::String& path) override;
#ifdef RIB_RMLUI_HEADLESS
      double clock_offset = 0;
      double GetElapsedTime() override;
#endif
   } system;
   void write_capture(int width, int height);
   std::unique_ptr<RominaboxRenderer> renderer;
   std::unique_ptr<TextInputPlatform> text_input;
   Rml::Context *context = nullptr;
   Rml::ElementDocument *document = nullptr;
   std::string asset_dir;
   std::string capture_path;
#ifdef RIB_RMLUI_HEADLESS
   unsigned texture_count = 0;
   unsigned geometry_count = 0;
#endif
};
}
