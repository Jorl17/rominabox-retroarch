#include "rmlui_bridge.h"

#include <RmlUi/Core.h>
#include <RmlUi/Core/Factory.h>
#include <RmlUi/Core/StringUtilities.h>
#include <RmlUi/Core/SystemInterface.h>
#include <filesystem>

#ifndef RIB_RMLUI_HEADLESS
#include <RmlUi_Renderer_GL2.h>
#include <RmlUi_Renderer_GL2.cpp>
#include <OpenGL/gl.h>
#include <OpenGL/glext.h>
#include "third_party/lodepng.h"
#endif

#include <sys/stat.h>

#include <algorithm>
#include <memory>
#include <cstdio>
#include <cstring>
#include <set>
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

/* The option the player chose with a click.
 *
 * We pass it as a string next to the action, not inside the action. Slots are
 * `SELECT_SLOT_1 + index`, and if we encoded every list that way, the number
 * of controllers for a console would have to be in the enum, while it is a
 * fact of the console package.
 */
static std::string chosen_device;

class DeviceOptionListener : public Rml::EventListener
{
public:
   explicit DeviceOptionListener(std::string id) : id(std::move(id)) {}

   void ProcessEvent(Rml::Event&) override
   {
      chosen_device = id;
      ActionListener::queue_action(RIB_RMLUI_ACTION_DEVICE_PICKER_CHOOSE);
   }
   void OnDetach(Rml::Element*) override { delete this; }

private:
   std::string id;
};

extern "C" const char *rib_rmlui_chosen_device(void)
{
   return chosen_device.c_str();
}

std::unique_ptr<RominaboxRenderer> renderer;
Rml::Context *context = nullptr;
Rml::ElementDocument *document = nullptr;
std::string asset_dir;
struct StatusMessage { std::string text; double expires = 0; };
StatusMessage main_status, controls_status;
/* Click an element by id, as with a pointer.
 *
 * With the offscreen renderer we can show what a state looks like, but not
 * that a player reaches that state through the bridge, which is not loaded
 * there. Here we send a Click to an element in the document, so the listener
 * that runs is the same as for a click with the mouse, such as ActionListener
 * or DeviceOptionListener, and not a class set for the picture.
 *
 * With this we can do nothing that a player cannot already do with a
 * pointer, because every element we can reach is already clickable.
 */
extern "C" bool rib_rmlui_click_element(const char *id)
{
   if (!document || !id || !*id)
      return false;
   Rml::Element *element = document->GetElementById(id);
   if (!element)
      return false;
   element->Click();
   return true;
}

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

/* Attach listeners to the control elements.
 *
 * We call this again once we know the control list. We load the document
 * before we read the controls configuration, so during the load the list is
 * empty and we attach nothing. Keyboard input does not go through these
 * listeners, so only clicks and hovers depend on this second call.
 */
/* Attach the controller picker, if there is a choice for this console.
 *
 * The options are in the document only when there is more than one pad, so a
 * missing picker is not an error. Most consoles have exactly one pad.
 */
extern "C" void rib_rmlui_wire_device_picker(void)
{
   if (!document)
      return;
   if (Rml::Element *current = document->GetElementById("controls-device-current"))
      current->AddEventListener(Rml::EventId::Click,
            new ActionListener(RIB_RMLUI_ACTION_DEVICE_PICKER_TOGGLE));

   for (int index = 0; index < rib_rmlui_device_count(); ++index)
   {
      const char *id = rib_rmlui_device_id(index);
      if (!id || !*id)
         continue;
      if (Rml::Element *option =
            document->GetElementById("controls-device-option-" + std::string(id)))
         option->AddEventListener(Rml::EventId::Click,
               new DeviceOptionListener(id));
   }
}

/* Show or hide the picker's list, and mark which option is in use. */
extern "C" void rib_rmlui_set_device_picker(bool open, const char *chosen)
{
   if (!document)
      return;
   if (Rml::Element *list = document->GetElementById("controls-device-list"))
   {
      if (open)
         list->RemoveProperty("display");
      else
         list->SetProperty("display", "none");
   }
   for (int index = 0; index < rib_rmlui_device_count(); ++index)
   {
      const char *id = rib_rmlui_device_id(index);
      if (!id || !*id)
         continue;
      if (Rml::Element *option =
            document->GetElementById("controls-device-option-" + std::string(id)))
         option->SetClass("selected", chosen && !std::strcmp(chosen, id));
   }
   if (Rml::Element *current = document->GetElementById("controls-device-current"))
      for (int index = 0; index < rib_rmlui_device_count(); ++index)
         if (chosen && rib_rmlui_device_id(index)
               && !std::strcmp(chosen, rib_rmlui_device_id(index)))
         {
            const char *name = rib_rmlui_device_name(index);
            current->SetInnerRML(Rml::StringUtilities::EncodeRml(name ? name : chosen));
            break;
         }
}

extern "C" void rib_rmlui_wire_controls(void)
{
   if (!document)
      return;
   /* Walk the elements in the document, not a list of ids.
    *
    * We generate the scene markup from the console package, so the elements
    * in the document are the declared controls, however many there are, and
    * their names are not in this code. */
   for (int index = 0; index < rib_rmlui_control_capacity(); ++index)
   {
      const char *control_id = rib_rmlui_control_id(index);
      if (!control_id || !*control_id)
         break;
      const int action = RIB_RMLUI_ACTION_CONTROL_FIRST + index;
      const std::string ids[] = {
         "control-" + std::string(control_id),
         "control-hit-" + std::string(control_id)
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

}

/* What we use for a design that declares nothing: the two built-in screens,
 * with the default wording of the player. They match
 * `built_in_screens()` in themes.rs. We use them in the headless interaction
 * tests, which load a document with no design declarations next to it. */
static void rib_rmlui_built_in_screens(void)
{
   /* The same path as for the declaration in a design, so we attach the
    * listeners to the buttons in the same way and only once. */
   rib_rmlui_declare_screen("pause", "pause-panel", "GAME PAUSED",
         "ESC  CONTINUE", "controls-back");
   rib_rmlui_declare_screen("controls", "controls-panel", "CONTROLS",
         "ESC  BACK", "controls");
}

bool load_document()
{
   document = context ? context->LoadDocument(asset_path("menu.rml")) : nullptr;
   if (!document)
      return false;

   /* `opens_screen` means we handle the click through the screen declaration
    * in the design, not through this table. Hover still comes from here,
    * because we track keyboard focus by action, and the player can also reach
    * the two buttons that change screen with the arrow keys. There is nothing
    * to add here for a screen that a design declares later. */
   struct Binding { const char *id; int action; bool opens_screen; };
   const Binding bindings[] = {
      {"resume", RIB_RMLUI_ACTION_RESUME, false},
      {"save", RIB_RMLUI_ACTION_SAVE, false},
      {"load", RIB_RMLUI_ACTION_LOAD, false},
      {"controls", RIB_RMLUI_ACTION_CONTROLS, true},
      {"quit", RIB_RMLUI_ACTION_QUIT, false},
      {"slot-1", RIB_RMLUI_ACTION_SELECT_SLOT_1, false},
      {"slot-2", RIB_RMLUI_ACTION_SELECT_SLOT_2, false},
      {"slot-3", RIB_RMLUI_ACTION_SELECT_SLOT_3, false},
      {"slot-4", RIB_RMLUI_ACTION_SELECT_SLOT_4, false},
      {"slot-5", RIB_RMLUI_ACTION_SELECT_SLOT_5, false},
      {"slot-6", RIB_RMLUI_ACTION_SELECT_SLOT_6, false},
      {"controls-back", RIB_RMLUI_ACTION_CONTROLS_BACK, true},
      {"controls-reset", RIB_RMLUI_ACTION_CONTROLS_RESET, false},
      {"controls-cancel", RIB_RMLUI_ACTION_CONTROLS_CANCEL, false}
   };

   for (const Binding& binding : bindings)
      if (Rml::Element *element = document->GetElementById(binding.id))
      {
         if (!binding.opens_screen)
            element->AddEventListener(Rml::EventId::Click,
                  new ActionListener(binding.action));
         element->AddEventListener(Rml::EventId::Mouseover,
               new HoverListener(binding.action));
         element->AddEventListener(Rml::EventId::Mouseout,
               new HoverListener(binding.action));
      }

   rib_rmlui_wire_controls();
   /* When a design declares screens, we replace these before the first frame,
    * and when it declares none we keep them. In both cases we attach the
    * listeners to the buttons before the player can press anything. */
   rib_rmlui_built_in_screens();

   update_document_state();
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

/* Where we write the next rendered frame. We set it in
 * rib_rmlui_capture_next() and clear it after the write. */
std::string capture_path;

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
static void rib_rmlui_write_capture(int width, int height)
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

extern "C" void rib_rmlui_capture_next(const char *path)
{
   capture_path = (path && *path) ? path : "";
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
   /* After the menu is drawn and before the frame is presented. */
   rib_rmlui_write_capture(width, height);
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

/* The screens declared in a design, in the order of declaration. */
struct Screen { std::string id, panel, heading, footer, button; };
std::vector<Screen> screens;
std::string requested_screen;
/* Buttons that already have a listener. We replace the built-in screens with
 * the declaration in a design, and without this we would attach two listeners
 * to the same button, and every press would send two intents. */
std::set<std::string> wired_screen_buttons;

/* Pressing the button for a screen. We pass the id next to the action, not
 * inside it, so declaring a screen never adds to the action enum. */
class ScreenListener : public Rml::EventListener
{
public:
   explicit ScreenListener(std::string id) : id(std::move(id)) {}
   void ProcessEvent(Rml::Event&) override
   {
      requested_screen = id;
      ActionListener::queue_action(RIB_RMLUI_ACTION_SHOW_SCREEN);
   }
   void OnDetach(Rml::Element*) override { delete this; }
private:
   std::string id;
};

/* The sound for an intent.
 *
 * We play a sound for every action and list the silent exceptions by name, so
 * an action added later has a sound by default, and we always make silence an
 * explicit choice.
 */
extern "C" enum rib_menu_sound rib_rmlui_action_sound(int action)
{
   switch (action)
   {
      case RIB_RMLUI_ACTION_NONE:
         return RIB_MENU_SOUND_NONE;
      /* Leaving a screen, rather than choosing something on it. */
      case RIB_RMLUI_ACTION_RESUME:
      case RIB_RMLUI_ACTION_CONTROLS_BACK:
      case RIB_RMLUI_ACTION_CONTROLS_CANCEL:
         return RIB_MENU_SOUND_CANCEL;
      /* Moving the highlight between save slots is navigation, and we already
       * play the movement cue for it, so a second sound would be one too many. */
      case RIB_RMLUI_ACTION_SELECT_SLOT_1:
      case RIB_RMLUI_ACTION_SELECT_SLOT_2:
      case RIB_RMLUI_ACTION_SELECT_SLOT_3:
      case RIB_RMLUI_ACTION_SELECT_SLOT_4:
      case RIB_RMLUI_ACTION_SELECT_SLOT_5:
      case RIB_RMLUI_ACTION_SELECT_SLOT_6:
         return RIB_MENU_SOUND_NONE;
      default:
         return RIB_MENU_SOUND_OK;
   }
}

extern "C" const char *rib_rmlui_requested_screen(void)
{
   return requested_screen.c_str();
}

/* What we use for a design that declares nothing: the two built-in screens,
 * with the default wording of the player. They match
 * `built_in_screens()` in themes.rs, and we use them in the headless
 * interaction tests, which load a document with no design declarations. */
extern "C" void rib_rmlui_clear_screens(void)
{
   screens.clear();
}

extern "C" void rib_rmlui_declare_screen(const char *id, const char *panel,
      const char *heading, const char *footer, const char *button)
{
   if (!id || !*id || !panel || !*panel)
      return;
   screens.push_back(Screen{id, panel, heading ? heading : "",
         footer ? footer : "", button ? button : ""});
   /* We load the document before we read a design, so we attach the listener
    * to the button here and not during the load. */
   if (document && button && *button
         && wired_screen_buttons.insert(button).second)
      if (Rml::Element *element = document->GetElementById(button))
         element->AddEventListener(Rml::EventId::Click, new ScreenListener(id));
}

/* Show one screen and hide the rest.
 *
 * We find screens by id, so there is nothing to change here for a new screen.
 *
 * The words of the heading and the footer are in the design. The player
 * contains none of them, just as it contains no control or controller names.
 * The text is in the design for the screen.
 */
extern "C" bool rib_rmlui_show_screen(const char *id)
{
   if (!document || !id || !*id)
      return false;
   const Screen *wanted = nullptr;
   for (const Screen& screen : screens)
      if (screen.id == id)
      {
         wanted = &screen;
         break;
      }
   if (!wanted)
      return false;

   rib_rmlui_clear_intents();
   for (const Screen& screen : screens)
      if (Rml::Element *panel = document->GetElementById(screen.panel))
      {
         if (&screen == wanted)
            panel->RemoveProperty("display");
         else
            panel->SetProperty("display", "none");
      }
   if (Rml::Element *heading = document->GetElementById("heading"))
      heading->SetInnerRML(Rml::StringUtilities::EncodeRml(wanted->heading));
   if (!wanted->footer.empty())
      if (Rml::Element *footer = document->GetElementById("footer-hint"))
         footer->SetInnerRML(Rml::StringUtilities::EncodeRml(wanted->footer));
   return true;
}

/* An alias: both names have the same behaviour. */
extern "C" void rib_rmlui_show_controls(bool visible)
{
   rib_rmlui_show_screen(visible ? "controls" : "pause");
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
