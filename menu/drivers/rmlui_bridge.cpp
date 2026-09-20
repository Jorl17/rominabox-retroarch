#include "rmlui_bridge.h"

#include <RmlUi/Core.h>
#include <RmlUi/Core/Factory.h>
#include <RmlUi/Core/StringUtilities.h>
#include <RmlUi/Core/SystemInterface.h>
#include <filesystem>

#ifndef RIB_RMLUI_HEADLESS
#include <RmlUi_Renderer_GL2.h>
#include "../../../vendor/RmlUi/Backends/RmlUi_Renderer_GL2.cpp"
#include <OpenGL/gl.h>
#include <OpenGL/glext.h>
#include "third_party/lodepng.h"
#endif

#include <sys/stat.h>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace
{
#ifdef RIB_RMLUI_HEADLESS
static unsigned test_texture_loads = 0;
class RominaboxRenderer : public Rml::RenderInterface
{
public:
   void SetViewport(int, int) {}
   void BeginFrame() {}
   void EndFrame() {}

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
      ++test_texture_loads;
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
   Rml::CompiledGeometryHandle geometry = 0;
};

#else
class RominaboxRenderer : public RenderInterface_GL2
{
public:
   void BeginFrame()
   {
      glGetIntegerv(GL_CURRENT_PROGRAM, &previous_program);
      glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &previous_array_buffer);
      glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &previous_element_buffer);
      glGetIntegerv(GL_MATRIX_MODE, &previous_matrix_mode);
      glPushAttrib(GL_ALL_ATTRIB_BITS);
      glPushClientAttrib(GL_CLIENT_ALL_ATTRIB_BITS);
      glMatrixMode(GL_PROJECTION);
      glPushMatrix();
      glMatrixMode(GL_MODELVIEW);
      glPushMatrix();
      glMatrixMode(GL_TEXTURE);
      glPushMatrix();
      glUseProgram(0);
      glBindBuffer(GL_ARRAY_BUFFER, 0);
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
      RenderInterface_GL2::BeginFrame();
   }

   void EndFrame()
   {
      RenderInterface_GL2::EndFrame();
      glMatrixMode(GL_TEXTURE);
      glPopMatrix();
      glMatrixMode(GL_MODELVIEW);
      glPopMatrix();
      glMatrixMode(GL_PROJECTION);
      glPopMatrix();
      glMatrixMode(previous_matrix_mode);
      glBindBuffer(GL_ARRAY_BUFFER, previous_array_buffer);
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, previous_element_buffer);
      glUseProgram(previous_program);
      glPopClientAttrib();
      glPopAttrib();
   }

   Rml::TextureHandle LoadTexture(Rml::Vector2i& dimensions,
         const Rml::String& source) override
   {
      std::vector<unsigned char> rgba;
      unsigned width = 0;
      unsigned height = 0;
      const unsigned error = lodepng::decode(rgba, width, height, source);
      if (error)
      {
         Rml::Log::Message(Rml::Log::LT_ERROR,
               "Could not decode PNG texture %s: %s",
               source.c_str(), lodepng_error_text(error));
         return {};
      }

      dimensions = {static_cast<int>(width), static_cast<int>(height)};
      return GenerateTexture(
            Rml::Span<const Rml::byte>(rgba.data(), rgba.size()), dimensions);
   }

private:
   GLint previous_program = 0;
   GLint previous_array_buffer = 0;
   GLint previous_element_buffer = 0;
   GLint previous_matrix_mode = GL_MODELVIEW;
};
#endif


#ifdef RIB_RMLUI_HEADLESS
static double test_clock_offset = 0;
#endif
class LocalSystemInterface : public Rml::SystemInterface
{
public:
#ifdef RIB_RMLUI_HEADLESS
   double GetElapsedTime() override { return Rml::SystemInterface::GetElapsedTime() + test_clock_offset; }
#endif
   void JoinPath(Rml::String& output, const Rml::String& document_path,
         const Rml::String& path) override
   {
      // These are filesystem resources, not web-root-relative URLs.
      const auto child = std::filesystem::u8path(path);
      const auto base = std::filesystem::u8path(document_path).parent_path();
      output = (child.is_absolute() ? child : base / child).lexically_normal().u8string();
   }
};
LocalSystemInterface system_interface;

class ActionListener : public Rml::EventListener
{
public:
   explicit ActionListener(int action) : action(action) {}

   void ProcessEvent(Rml::Event& event) override
   {
      if (Rml::Element *element = event.GetCurrentElement())
      {
         if (element->HasAttribute("disabled") ||
             element->IsClassSet("disabled"))
            return;
      }
      queue_action(action);
   }
   void OnDetach(Rml::Element*) override { delete this; }

   static void queue_action(int action)
   {
      if (action == RIB_RMLUI_ACTION_NONE || count >= capacity)
         return;
      items[(head + count) % capacity] = action;
      ++count;
   }

   static int take_action()
   {
      if (count <= 0)
         return RIB_RMLUI_ACTION_NONE;
      const int action = items[head];
      head = (head + 1) % capacity;
      --count;
      return action;
   }

   static void clear()
   {
      head = 0;
      count = 0;
   }

private:
   int action;
   static constexpr int capacity = 8;
   static int items[capacity];
   static int head;
   static int count;
};

int ActionListener::items[ActionListener::capacity] = {};
int ActionListener::head = 0;
int ActionListener::count = 0;

class HoverListener : public Rml::EventListener
{
public:
   explicit HoverListener(int action) : action(action) {}

   void ProcessEvent(Rml::Event& event) override
   {
      if (event.GetId() == Rml::EventId::Mouseout)
      {
         if (hovered_action == action)
            hovered_action = RIB_RMLUI_ACTION_NONE;
         return;
      }
      hovered_action = action;
   }
   void OnDetach(Rml::Element*) override { delete this; }

   static int hovered_action;

private:
   int action;
};

int HoverListener::hovered_action = RIB_RMLUI_ACTION_NONE;

std::unique_ptr<RominaboxRenderer> renderer;
Rml::Context *context = nullptr;
Rml::ElementDocument *document = nullptr;
std::string asset_dir;
struct StatusMessage { std::string text; double expires = 0; };
StatusMessage main_status, controls_status;
void show_status(StatusMessage& message, const char *id, const char *text)
{
   message.text = text ? text : "";
   message.expires = system_interface.GetElapsedTime() + 5.0;
   if (document)
      if (auto *element = document->GetElementById(id))
         element->SetInnerRML(Rml::StringUtilities::EncodeRml(message.text));
}
void expire_status(StatusMessage& message, const char *id)
{
   if (!message.text.empty() && system_interface.GetElapsedTime() >= message.expires)
      show_status(message, id, "");
}
float game_aspect = 4.0f / 3.0f;
int selected_slot = 1;
int focused_item = RIB_RMLUI_ACTION_RESUME;
struct SlotState
{
   bool occupied = false;
   std::string thumbnail_path;
   std::string thumbnail_version;
};
SlotState slots[6];
time_t rml_mtime = 0;
time_t rcss_mtime = 0;
bool pointer_down = false;

time_t modification_time(const std::string& path)
{
   struct stat info = {};
   return stat(path.c_str(), &info) == 0 ? info.st_mtime : 0;
}

std::string thumbnail_version(const std::string& path)
{
   struct stat info = {};
   if (path.empty() || stat(path.c_str(), &info) != 0) return {};
   long nanoseconds = 0;
#if defined(__APPLE__)
   nanoseconds = info.st_mtimespec.tv_nsec;
#elif !defined(_WIN32)
   nanoseconds = info.st_mtim.tv_nsec;
#endif
   return std::to_string(info.st_mtime) + ":" +
      std::to_string(nanoseconds) + ":" + std::to_string(info.st_size);
}

bool thumbnail_ready(const std::string& path)
{
#ifndef RIB_RMLUI_HEADLESS
   // The screenshot is written asynchronously. Never pass RmlUi a partial PNG,
   // because failed loads stay in its file-texture cache until a context rebuild.
   std::vector<unsigned char> pixels;
   unsigned width = 0, height = 0;
   return lodepng::decode(pixels, width, height, path) == 0;
#else
   return !path.empty();
#endif
}

std::string asset_path(const char *name)
{
   return asset_dir + "/" + name;
}

std::string quoted_css_path(const std::string& path)
{
   std::string result;
   for (char value : path)
   {
      if (value == '\\' || value == '"') result += '\\';
      result += value;
   }
   return result;
}

void update_document_state()
{
   if (!document)
      return;

   struct ActionElement { const char *id; int action; };
   const ActionElement action_elements[] = {
      {"resume", RIB_RMLUI_ACTION_RESUME},
      {"save", RIB_RMLUI_ACTION_SAVE},
      {"load", RIB_RMLUI_ACTION_LOAD},
      {"controls", RIB_RMLUI_ACTION_CONTROLS},
      {"quit", RIB_RMLUI_ACTION_QUIT}
   };

   for (const ActionElement& action : action_elements)
      if (Rml::Element *element = document->GetElementById(action.id))
         element->SetClass("focused", action.action == focused_item);

   for (int index = 0; index < 6; ++index)
   {
      const int slot = index + 1;
      const std::string suffix = std::to_string(slot);
      if (Rml::Element *element = document->GetElementById("slot-" + suffix))
      {
         element->SetClass("selected", slot == selected_slot);
         element->SetClass("focused",
               focused_item == RIB_RMLUI_ACTION_SELECT_SLOT_1 + index);
         element->SetClass("occupied", slots[index].occupied);
         element->SetClass("empty", !slots[index].occupied);
      }
      if (Rml::Element *label = document->GetElementById("slot-label-" + suffix))
         label->SetInnerRML("SLOT " + suffix);
      if (Rml::Element *state = document->GetElementById("slot-state-" + suffix))
         state->SetInnerRML(slots[index].occupied ? "OCCUPIED" : "EMPTY");
      if (Rml::Element *image = document->GetElementById("slot-image-" + suffix))
      {
         // The core DAR corrects non-square source pixels. The well has that
         // ratio, and we fill it in RmlUi without a second letterbox.
         const float height = std::min(138.0f, 230.0f / game_aspect);
         if (auto *picture = image->GetParentNode())
         {
            picture->SetProperty("width", std::to_string(height * game_aspect) + "dp");
            picture->SetProperty("height", std::to_string(height) + "dp");
            picture->SetProperty("margin-top", std::to_string((138.0f - height) / 2) + "dp");
            picture->SetProperty("margin-bottom", std::to_string((138.0f - height) / 2) + "dp");
         }
         if (slots[index].occupied && !slots[index].thumbnail_path.empty())
            image->SetProperty("decorator", "image(\"" +
                  quoted_css_path(slots[index].thumbnail_path) + "\" fill)");
         else
            image->RemoveProperty("decorator");
      }
   }

   if (Rml::Element *load = document->GetElementById("load"))
   {
      const bool disabled = !slots[selected_slot - 1].occupied;
      load->SetClass("disabled", disabled);
      if (disabled)
         load->SetAttribute("disabled", "disabled");
      else
         load->RemoveAttribute("disabled");
   }

   if (Rml::Element *status = document->GetElementById("status"))
      status->SetInnerRML(Rml::StringUtilities::EncodeRml(main_status.text));
}

bool load_document()
{
   document = context ? context->LoadDocument(asset_path("menu.rml")) : nullptr;
   if (!document)
      return false;

   struct Binding { const char *id; int action; };
   const Binding bindings[] = {
      {"resume", RIB_RMLUI_ACTION_RESUME},
      {"save", RIB_RMLUI_ACTION_SAVE},
      {"load", RIB_RMLUI_ACTION_LOAD},
      {"controls", RIB_RMLUI_ACTION_CONTROLS},
      {"quit", RIB_RMLUI_ACTION_QUIT},
      {"slot-1", RIB_RMLUI_ACTION_SELECT_SLOT_1},
      {"slot-2", RIB_RMLUI_ACTION_SELECT_SLOT_2},
      {"slot-3", RIB_RMLUI_ACTION_SELECT_SLOT_3},
      {"slot-4", RIB_RMLUI_ACTION_SELECT_SLOT_4},
      {"slot-5", RIB_RMLUI_ACTION_SELECT_SLOT_5},
      {"slot-6", RIB_RMLUI_ACTION_SELECT_SLOT_6},
      {"controls-back", RIB_RMLUI_ACTION_CONTROLS_BACK},
      {"controls-reset", RIB_RMLUI_ACTION_CONTROLS_RESET},
      {"controls-cancel", RIB_RMLUI_ACTION_CONTROLS_CANCEL}
   };

   for (const Binding& binding : bindings)
      if (Rml::Element *element = document->GetElementById(binding.id))
      {
         element->AddEventListener(Rml::EventId::Click,
               new ActionListener(binding.action));
         element->AddEventListener(Rml::EventId::Mouseover,
               new HoverListener(binding.action));
         element->AddEventListener(Rml::EventId::Mouseout,
               new HoverListener(binding.action));
      }

   const char *control_ids[] = {
      "up", "down", "left", "right", "a", "b",
      "x", "y", "l", "r", "l2", "r2", "l3", "r3",
      "start", "select"
   };
   for (int index = 0; index < 16; ++index)
   {
      const int action = RIB_RMLUI_ACTION_CONTROL_FIRST + index;
      const std::string ids[] = {
         "control-" + std::string(control_ids[index]),
         "control-hit-" + std::string(control_ids[index])
      };
      for (const std::string& id : ids)
         if (Rml::Element *element = document->GetElementById(id))
         {
            element->AddEventListener(Rml::EventId::Click,
                  new ActionListener(action));
            element->AddEventListener(Rml::EventId::Mouseover,
                  new HoverListener(action));
            element->AddEventListener(Rml::EventId::Mouseout,
                  new HoverListener(action));
         }
   }

   update_document_state();
   if (Rml::Element *edit = document->GetElementById("controls-edit-label"))
      edit->SetProperty("display", "none");
   if (Rml::Element *dialog = document->GetElementById("controls-label-dialog"))
      dialog->SetProperty("display", "none");
   document->Show();
   rml_mtime = modification_time(asset_path("menu.rml"));
   rcss_mtime = modification_time(asset_path("menu.rcss"));
   return true;
}
}

extern "C" bool rib_rmlui_init(
      const char *asset_directory, int width, int height)
{
   if (context)
      return true;
   if (!asset_directory || !*asset_directory)
      return false;

   asset_dir = asset_directory;
   renderer = std::make_unique<RominaboxRenderer>();
   renderer->SetViewport(width, height);
   Rml::SetSystemInterface(&system_interface);
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
   if (!context || !load_document())
   {
      Rml::Shutdown();
      context = nullptr;
      document = nullptr;
      renderer.reset();
      return false;
   }
   context->SetDensityIndependentPixelRatio(1.0f);
   context->Update();
   return true;
}

extern "C" void rib_rmlui_shutdown(void)
{
   if (context)
      Rml::RemoveContext("rominabox-menu");
   context = nullptr;
   document = nullptr;
   if (renderer)
      Rml::Shutdown();
   renderer.reset();
   ActionListener::clear();
   HoverListener::hovered_action = RIB_RMLUI_ACTION_NONE;
   pointer_down = false;
}

extern "C" void rib_rmlui_render(int width, int height)
{
   if (!context || !renderer)
      return;
   expire_status(main_status, "status");
   expire_status(controls_status, "controls-status");
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
}

extern "C" void rib_rmlui_set_selected_slot(int slot)
{
   if (slot < 1 || slot > 6)
      return;
   selected_slot = slot;
   update_document_state();
}

extern "C" void rib_rmlui_set_focused(int focused)
{
   focused_item = focused;
   update_document_state();
}

extern "C" void rib_rmlui_set_slot_state(int slot, bool occupied,
      const char *thumbnail_path)
{
   if (slot < 1 || slot > 6)
      return;
   SlotState& state = slots[slot - 1];
   const std::string next_path = thumbnail_path ? thumbnail_path : "";
   const std::string version = thumbnail_version(next_path);
   if (state.occupied == occupied && state.thumbnail_path == next_path &&
         state.thumbnail_version == version)
      return;
   state.occupied = occupied;
   if (!next_path.empty() && !thumbnail_ready(next_path))
   {
      update_document_state();
      return;
   }
   if (!state.thumbnail_path.empty() && renderer)
      Rml::ReleaseTexture(state.thumbnail_path, renderer.get());
   state.thumbnail_path = next_path;
   state.thumbnail_version = version;
   update_document_state();
}

extern "C" void rib_rmlui_set_game_aspect(float aspect)
{
   if (!(aspect > 0.0f && aspect < 100.0f) || aspect == game_aspect)
      return;
   game_aspect = aspect;
   update_document_state();
}

extern "C" void rib_rmlui_set_status(const char *status)
{
   show_status(main_status, "status", status);
}

extern "C" void rib_rmlui_show_controls(bool visible)
{
   rib_rmlui_clear_intents();
   if (!document)
      return;
   if (Rml::Element *pause = document->GetElementById("pause-panel"))
   {
      if (visible)
         pause->SetProperty("display", "none");
      else
         pause->RemoveProperty("display");
   }
   if (Rml::Element *controls = document->GetElementById("controls-panel"))
   {
      if (visible)
         controls->RemoveProperty("display");
      else
         controls->SetProperty("display", "none");
   }
   if (Rml::Element *heading = document->GetElementById("heading"))
      heading->SetInnerRML(visible ? "CONTROLS" : "GAME PAUSED");
}

extern "C" void rib_rmlui_set_control_state(const char *id,
      const char *label, const char *binding, bool focused, bool capturing)
{
   if (!document || !id)
      return;
   const std::string suffix(id);
   if (Rml::Element *control = document->GetElementById("control-" + suffix))
   {
      control->SetClass("focused", focused);
      control->SetClass("capturing", capturing);
   }
   if (Rml::Element *hit = document->GetElementById("control-hit-" + suffix))
   {
      hit->SetClass("focused", focused);
      hit->SetClass("capturing", capturing);
   }
   if (Rml::Element *label_element =
         document->GetElementById("control-label-" + suffix))
      label_element->SetInnerRML(Rml::StringUtilities::EncodeRml(
            label ? label : ""));
   if (Rml::Element *binding_element =
         document->GetElementById("control-binding-" + suffix))
      binding_element->SetInnerRML(Rml::StringUtilities::EncodeRml(
            binding ? binding : ""));
}

extern "C" void rib_rmlui_set_controls_status(const char *status)
{
   show_status(controls_status, "controls-status", status);
}

extern "C" void rib_rmlui_set_controls_action_focus(
      bool reset, bool back, bool cancel)
{
   if (!document)
      return;
   if (Rml::Element *element = document->GetElementById("controls-edit-label"))
   {
      element->SetClass("focused", false);
      element->SetProperty("display", "none");
   }
   if (Rml::Element *element = document->GetElementById("controls-reset"))
      element->SetClass("focused", reset);
   if (Rml::Element *element = document->GetElementById("controls-back"))
      element->SetClass("focused", back);
   if (Rml::Element *element = document->GetElementById("controls-cancel"))
   {
      element->SetClass("focused", cancel);
      if (cancel)
         element->RemoveProperty("display");
      else
         element->SetProperty("display", "none");
   }
}

extern "C" void rib_rmlui_set_label_dialog(bool visible, const char *value)
{
   rib_rmlui_clear_intents();
   if (!document)
      return;
   if (Rml::Element *dialog = document->GetElementById("controls-label-dialog"))
   {
      if (visible)
         dialog->RemoveProperty("display");
      else
         dialog->SetProperty("display", "none");
   }
   if (Rml::Element *element = document->GetElementById("controls-label-value"))
      element->SetInnerRML(Rml::StringUtilities::EncodeRml(
            value ? value : ""));
}

extern "C" void rib_rmlui_set_footer_hint(const char *hint)
{
   if (!document)
      return;
   if (Rml::Element *element = document->GetElementById("footer-hint"))
      element->SetInnerRML(Rml::StringUtilities::EncodeRml(
            hint ? hint : ""));
}

extern "C" void rib_rmlui_set_splash(bool visible, float opacity)
{
   rib_rmlui_clear_intents();
   if (!document)
      return;
   const char *hidden_ids[] = {"heading", "pause-panel", "footer"};
   for (const char *id : hidden_ids)
      if (Rml::Element *element = document->GetElementById(id))
      {
         if (visible)
            element->SetProperty("display", "none");
         else
            element->RemoveProperty("display");
      }
   if (Rml::Element *body = document->GetElementById("body"))
   {
      if (visible)
         body->SetProperty("background-color", "transparent");
      else
         body->RemoveProperty("background-color");
   }
   if (Rml::Element *screen = document->GetElementById("screen"))
   {
      if (visible)
      {
         screen->SetProperty("background-color", "transparent");
         screen->SetProperty("border-color", "transparent");
         screen->SetProperty("decorator", "none");
      }
      else
      {
         screen->RemoveProperty("background-color");
         screen->RemoveProperty("border-color");
         screen->RemoveProperty("decorator");
      }
   }
   if (Rml::Element *splash = document->GetElementById("splash"))
   {
      if (visible)
      {
         if (Rml::Element *logo = document->GetElementById("splash-logo"))
            if (!logo->HasAttribute("src"))
               logo->SetAttribute("src", "splash-logo.png");
         splash->SetProperty("display", "block");
         splash->SetProperty("opacity", std::to_string(opacity));
      }
      else
         splash->SetProperty("display", "none");
   }
}

extern "C" void rib_rmlui_pointer_move(int x, int y)
{
   if (context)
      context->ProcessMouseMove(x, y, 0);
}

extern "C" void rib_rmlui_pointer_button(bool down)
{
   if (!context)
      return;
   if (down == pointer_down)
      return;
   pointer_down = down;
   if (down)
      context->ProcessMouseButtonDown(0, 0);
   else
      context->ProcessMouseButtonUp(0, 0);
}

extern "C" int rib_rmlui_take_action(void)
{
   return ActionListener::take_action();
}

extern "C" int rib_rmlui_hovered_action(void)
{
   return HoverListener::hovered_action;
}

extern "C" void rib_rmlui_clear_intents(void)
{
   ActionListener::clear();
   HoverListener::hovered_action = RIB_RMLUI_ACTION_NONE;
}

extern "C" void rib_rmlui_pointer_leave(void)
{
   if (!context)
      return;
   context->ProcessMouseLeave();
   HoverListener::hovered_action = RIB_RMLUI_ACTION_NONE;
   if (pointer_down)
   {
      pointer_down = false;
      context->ProcessMouseButtonUp(0, 0);
   }
}

extern "C" bool rib_rmlui_element_center(const char *id, int *x, int *y)
{
   if (!context || !document || !id || !x || !y)
      return false;
   context->Update();
   Rml::Element *element = document->GetElementById(id);
   if (!element)
      return false;
   const Rml::Vector2f offset = element->GetAbsoluteOffset(Rml::BoxArea::Border);
   const Rml::Vector2f size = element->GetBox().GetSize(Rml::BoxArea::Border);
   *x = static_cast<int>(offset.x + size.x * 0.5f);
   *y = static_cast<int>(offset.y + size.y * 0.5f);
   return size.x > 0.f && size.y > 0.f;
}

extern "C" bool rib_rmlui_element_disabled(const char *id)
{
   if (!document || !id)
      return false;
   Rml::Element *element = document->GetElementById(id);
   return element && element->HasAttribute("disabled");
}

extern "C" bool rib_rmlui_reload_if_changed(void)
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
   return load_document();
}

#ifdef RIB_RMLUI_HEADLESS
extern "C" const char *rib_rmlui_test_property(const char *id, const char *property)
{
   static std::string value;
   context->Update();
   auto *element = document->GetElementById(id);
   value = element && element->GetProperty(property) ? element->GetProperty(property)->ToString() : "";
   return value.c_str();
}
#endif

#ifdef RIB_RMLUI_HEADLESS
extern "C" unsigned rib_rmlui_test_texture_loads() { return test_texture_loads; }
#endif

#ifdef RIB_RMLUI_HEADLESS
extern "C" void rib_rmlui_test_advance(double seconds) { test_clock_offset += seconds; }
extern "C" const char *rib_rmlui_test_text(const char *id) {
   static std::string text;
   text = document->GetElementById(id)->GetInnerRML(); return text.c_str();
}
extern "C" float rib_rmlui_test_picture_aspect() {
   context->Update(); auto size = document->GetElementById("slot-image-1")->GetParentNode()->GetBox().GetSize(Rml::BoxArea::Content); return size.x / size.y;
}
#endif
