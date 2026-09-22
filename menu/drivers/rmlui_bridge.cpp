#include "rmlui_bridge.h"

#include <RmlUi/Core.h>
#include <RmlUi/Core/Factory.h>
#include <RmlUi/Core/ElementUtilities.h>
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
#include <cmath>
#include <map>
#include <memory>
#include <cstdio>
#include <cstring>
#include <set>
#include <string>
#include <vector>

extern "C" void rib_rmlui_wire_toggles(void);
static void wire_arrows(Rml::Element *node);

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
/* Which button on the pause row is focused, by id.
 *
 * The pause row contains what the design and the export put in it. With
 * Options on, the fourth button is `options`, not `controls`. So we track the
 * focus by element id and not by a fixed table of actions. */
std::string focused_element = "resume";
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

/* Why the save slots are unusable, in the words of the design: the word on
 * each slot and the line that explains it. Empty means they are usable. The
 * decision is elsewhere in the player, and here we only draw it. */
std::string slots_guard;
std::string slots_guard_reason;

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

   /* The contents of the pause panel, in document order. */
   char row[16][64];
   const int row_count = rib_rmlui_focusables("pause-panel", row, 16);
   for (int index = 0; index < row_count; ++index)
      if (Rml::Element *element = document->GetElementById(row[index]))
         element->SetClass("focused", focused_element == row[index]);

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
         element->SetClass("disabled", !slots_guard.empty());
         if (slots_guard.empty())
            element->RemoveAttribute("disabled");
         else
            element->SetAttribute("disabled", "disabled");
      }
      if (Rml::Element *label = document->GetElementById("slot-label-" + suffix))
         label->SetInnerRML("SLOT " + suffix);
      if (Rml::Element *state = document->GetElementById("slot-state-" + suffix))
         state->SetInnerRML(slots_guard.empty()
               ? (slots[index].occupied ? "OCCUPIED" : "EMPTY")
               : Rml::StringUtilities::EncodeRml(slots_guard));
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

   for (const char *id : {"save", "load"})
      if (Rml::Element *button = document->GetElementById(id))
      {
         const bool disabled = !slots_guard.empty() ||
               (std::string(id) == "load" && !slots[selected_slot - 1].occupied);
         button->SetClass("disabled", disabled);
         if (disabled)
            button->SetAttribute("disabled", "disabled");
         else
            button->RemoveAttribute("disabled");
      }

   if (Rml::Element *status = document->GetElementById("status"))
   {
      /* Why the slots are unusable is a lasting state, not a message, so we keep
       * it while they are locked. Messages go in front of it and expire. */
      const std::string &shown =
         main_status.text.empty() ? slots_guard_reason : main_status.text;
      status->SetInnerRML(Rml::StringUtilities::EncodeRml(shown));
   }
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
   std::vector<std::string> wired_groups;
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
      const char *group = rib_rmlui_control_group(index);
      if (!group || !*group)
         continue;
      const std::string name(group);
      bool seen = false;
      for (const std::string& wired : wired_groups)
         if (wired == name)
            seen = true;
      if (seen)
         continue;
      wired_groups.push_back(name);
      if (Rml::Element *element = document->GetElementById("control-group-" + name))
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
   rib_rmlui_wire_toggles();
   wire_arrows(document);
   rib_rmlui_wire_lists();
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

static Rml::Element *slider_drag = nullptr;

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
   slider_drag = nullptr;
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
   /* A slot has the focus, so no button on the row does. */
   focused_element.clear();
   update_document_state();
}

/* Focus one button of the pause row, by its id in the document. */
extern "C" void rib_rmlui_focus_element(const char *id)
{
   focused_element = id ? id : "";
   /* No slot is focused either. The slot ids start after the row actions, so
    * any value outside that range means "none of them". */
   focused_item = RIB_RMLUI_ACTION_RESUME;
   update_document_state();
}

extern "C" const char *rib_rmlui_focused_element(void)
{
   return focused_element.c_str();
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
std::string chosen_item;
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
      /* A slider step is navigation. We play the up or down cue of the sound
       * pack when the level changes. Confirm as well would be a second sound,
       * even at an end where the level did not move. */
      case RIB_RMLUI_ACTION_SLIDER:
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

/* Replace the drawing in the controller scene.
 *
 * The export contains one of these for each pad in the picker, because we
 * cannot generate markup in the player. A different controller changes the
 * emulated device and the label, and with this call we redraw the pad.
 */
extern "C" bool rib_rmlui_set_scene(const char *markup)
{
   if (!document || !markup)
      return false;
   Rml::Element *scene = document->GetElementById("controller-scene");
   if (!scene)
      return false;
   scene->SetInnerRML(markup);
   return true;
}

extern "C" const char *rib_rmlui_requested_screen(void)
{
   return requested_screen.c_str();
}

extern "C" void rib_rmlui_remember_item(const char *id)
{
   chosen_item = id ? id : "";
}

extern "C" const char *rib_rmlui_chosen_item(void)
{
   return chosen_item.c_str();
}

/* A generated list is one class of row and one class of page. The list is on
 * whichever panel is shown, and in the bridge we do not know whether the rows
 * are shaders, achievements or anything else. */
static bool rib_display_none(Rml::Element *element)
{
   const Rml::Property *property = element ? element->GetProperty("display") : nullptr;
   return property && property->ToString() == "none";
}

static bool rib_hidden(Rml::Element *element)
{
   for (Rml::Element *cursor = element; cursor; cursor = cursor->GetParentNode())
      if (rib_display_none(cursor))
         return true;
   return false;
}

static void rib_collect(Rml::Element *element, const char *class_name,
      std::vector<Rml::Element*> &out)
{
   if (!element)
      return;
   if (element->IsClassSet(class_name))
      out.push_back(element);
   for (int index = 0; index < element->GetNumChildren(); ++index)
      rib_collect(element->GetChild(index), class_name, out);
}

static bool rib_under_class_hidden(Rml::Element *element, const char *class_name)
{
   for (Rml::Element *cursor = element; cursor; cursor = cursor->GetParentNode())
      if (cursor->IsClassSet(class_name) && rib_display_none(cursor))
         return true;
   return false;
}

static Rml::Element *rib_visible_list(void)
{
   if (!document)
      return nullptr;
   std::vector<Rml::Element*> lists;
   rib_collect(document, "list", lists);
   for (Rml::Element *list : lists)
      if (!rib_hidden(list))
         return list;
   return nullptr;
}

static void rib_visible_rows(std::vector<Rml::Element*> &rows)
{
   rows.clear();
   Rml::Element *list = rib_visible_list();
   if (!list)
      return;
   std::vector<Rml::Element*> all;
   rib_collect(list, "list-row", all);
   for (Rml::Element *row : all)
      if (!rib_under_class_hidden(row, "list-page") && !rib_display_none(row))
         rows.push_back(row);
}

class ListListener : public Rml::EventListener
{
public:
   enum Kind { Choose, Page };
   ListListener(Kind kind, std::string page) : kind(kind), page(std::move(page)) {}
   void ProcessEvent(Rml::Event& event) override
   {
      Rml::Element *element = event.GetCurrentElement();
      if (!element)
         return;
      if (element->HasAttribute("disabled") || element->IsClassSet("disabled"))
         return;
      chosen_item = kind == Choose ? std::string(element->GetId()) : page;
      ActionListener::queue_action(kind == Choose
            ? RIB_RMLUI_ACTION_LIST_CHOOSE
            : RIB_RMLUI_ACTION_LIST_PAGE);
   }
   void OnDetach(Rml::Element*) override { delete this; }
private:
   Kind kind;
   std::string page;
};

/* A switch is a button with the list-toggle class. We pass the pressed switch
 * next to the action, as for a row and a controller, so there are no switches
 * in the bridge. */
class ToggleListener : public Rml::EventListener
{
public:
   void ProcessEvent(Rml::Event& event) override
   {
      Rml::Element *element = event.GetCurrentElement();
      if (!element)
         return;
      if (element->HasAttribute("disabled") || element->IsClassSet("disabled"))
         return;
      chosen_item = element->GetId();
      ActionListener::queue_action(RIB_RMLUI_ACTION_TOGGLE);
   }
   void OnDetach(Rml::Element*) override { delete this; }
};

static void wire_part_toggles(Rml::Element *node);

extern "C" void rib_rmlui_wire_toggles(void)
{
   if (!document)
      return;
   std::vector<Rml::Element*> toggles;
   rib_collect(document, "list-toggle", toggles);
   for (Rml::Element *toggle : toggles)
      toggle->AddEventListener(Rml::EventId::Click, new ToggleListener());
   wire_part_toggles(document);
}

extern "C" void rib_rmlui_set_toggle(const char *id, const char *state, bool on)
{
   if (!document || !id || !*id)
      return;
   if (Rml::Element *toggle = document->GetElementById(id))
      toggle->SetClass("on", on);
   if (Rml::Element *word = document->GetElementById(std::string(id) + "-state"))
      word->SetInnerRML(Rml::StringUtilities::EncodeRml(state ? state : ""));
}

extern "C" void rib_rmlui_guard_slots(const char *label, const char *reason)
{
   const std::string next = label ? label : "";
   const std::string why = reason ? reason : "";
   if (next == slots_guard && why == slots_guard_reason)
      return;
   slots_guard = next;
   slots_guard_reason = why;
   update_document_state();
}

extern "C" bool rib_rmlui_slots_guarded(void)
{
   return !slots_guard.empty();
}

extern "C" void rib_rmlui_wire_lists(void)
{
   if (!document)
      return;
   std::vector<Rml::Element*> rows;
   rib_collect(document, "list-row", rows);
   for (Rml::Element *row : rows)
      row->AddEventListener(Rml::EventId::Click,
            new ListListener(ListListener::Choose, ""));
   std::vector<Rml::Element*> previous;
   rib_collect(document, "list-pager-prev", previous);
   for (Rml::Element *button : previous)
      button->AddEventListener(Rml::EventId::Click,
            new ListListener(ListListener::Page, "prev"));
   std::vector<Rml::Element*> next;
   rib_collect(document, "list-pager-next", next);
   for (Rml::Element *button : next)
      button->AddEventListener(Rml::EventId::Click,
            new ListListener(ListListener::Page, "next"));
}

extern "C" int rib_rmlui_visible_row_count(void)
{
   std::vector<Rml::Element*> rows;
   rib_visible_rows(rows);
   return (int)rows.size();
}

extern "C" void rib_rmlui_focus_list_row(int index)
{
   if (!document)
      return;
   std::vector<Rml::Element*> all;
   rib_collect(document, "list-row", all);
   for (Rml::Element *row : all)
      row->SetClass("focused", false);
   std::vector<Rml::Element*> rows;
   rib_visible_rows(rows);
   if (index >= 0 && index < (int)rows.size())
      rows[index]->SetClass("focused", true);
}

static Rml::Element *rib_visible_panel(void)
{
   if (!document)
      return nullptr;
   std::vector<Rml::Element*> panels;
   rib_collect(document, "screen-panel", panels);
   for (Rml::Element *panel : panels)
      if (!rib_display_none(panel))
         return panel;
   return nullptr;
}

/* The controls on the screen shown that the player can reach with the keyboard,
 * after its rows and in the order we draw them. We walk Options as a list,
 * because its entries are buttons on a panel, and a player with a pad could not
 * use a control that only a pointer can reach. */
static void rib_visible_controls(std::vector<Rml::Element*> &out)
{
   out.clear();
   Rml::Element *panel = rib_visible_panel();
   if (!panel)
      return;
   for (const char *name : {"option-entry", "list-toggle", "list-back", "options-back"})
      rib_collect(panel, name, out);
}

/* Press the way back from the screen shown, through the listener of the
 * element. The screen it returns to is in the declaration of the design, and
 * we do not work it out in the player. */
extern "C" bool rib_rmlui_click_screen_back(void)
{
   Rml::Element *panel = rib_visible_panel();
   if (!panel)
      return false;
   std::vector<Rml::Element*> back;
   rib_collect(panel, "list-back", back);
   rib_collect(panel, "options-back", back);
   if (back.empty())
      return false;
   return rib_rmlui_click_element(back.front()->GetId().c_str());
}

/* The button on the pause row that opens a screen.
 *
 * The fourth button of the pause row is for the screen the design puts there,
 * and with Options in the game it is not the controls button. So to focus and
 * press it, we find the element and do not use the name of a screen.
 */
extern "C" const char *rib_rmlui_pause_screen_button(void)
{
   static std::string id;
   id.clear();
   if (!document || screens.empty())
      return id.c_str();
   Rml::Element *pause = document->GetElementById(screens.front().panel);
   if (!pause)
      return id.c_str();
   for (const Screen& screen : screens)
   {
      const char *cursor = screen.button.c_str();
      while (*cursor)
      {
         while (*cursor == ' ')
            ++cursor;
         const char *end = cursor;
         while (*end && *end != ' ')
            ++end;
         if (end > cursor)
         {
            const std::string one(cursor, end);
            for (Rml::Element *e = document->GetElementById(one); e;
                  e = e->GetParentNode())
               if (e == pause)
               {
                  id = one;
                  return id.c_str();
               }
         }
         cursor = end;
      }
   }
   return id.c_str();
}

extern "C" int rib_rmlui_list_control_count(void)
{
   std::vector<Rml::Element*> controls;
   rib_visible_controls(controls);
   return (int)controls.size();
}

extern "C" const char *rib_rmlui_list_control_id(int index)
{
   static std::string id;
   std::vector<Rml::Element*> controls;
   rib_visible_controls(controls);
   id.clear();
   if (index >= 0 && index < (int)controls.size())
      id = controls[index]->GetId();
   return id.c_str();
}

extern "C" void rib_rmlui_focus_list_control(int index)
{
   std::vector<Rml::Element*> controls;
   rib_visible_controls(controls);
   for (size_t at = 0; at < controls.size(); ++at)
      controls[at]->SetClass("focused", (int)at == index);
}

extern "C" const char *rib_rmlui_list_row_id(int index)
{
   static std::string id;
   std::vector<Rml::Element*> rows;
   rib_visible_rows(rows);
   id.clear();
   if (index >= 0 && index < (int)rows.size())
      id = rows[index]->GetId();
   return id.c_str();
}

static bool rib_page_has_row(Rml::Element *page)
{
   std::vector<Rml::Element*> rows;
   rib_collect(page, "list-row", rows);
   for (Rml::Element *row : rows)
   {
      /* The page itself may be hidden. The computed display would then be none
       * for every row on it, so a later page with a binding would look empty.
       * We would never show the pager, and the player could not reach that
       * binding. Only a display set on the row itself counts. */
      const Rml::Property *property = row->GetLocalProperty("display");
      if (!property || property->ToString() != "none")
         return true;
   }
   return false;
}

/* We draw an arrow with no page behind it as inactive, and it has no effect.
 * With wrapping, both arrows would look active on every page. */
static void rib_mark_pager(Rml::Element *list, int page, int pages)
{
   struct Arrow { const char *name; bool dead; };
   const Arrow arrows[] = {
      {"list-pager-prev", page <= 0},
      {"list-pager-next", page >= pages - 1}
   };
   for (const Arrow& arrow : arrows)
   {
      std::vector<Rml::Element*> found;
      rib_collect(list, arrow.name, found);
      for (Rml::Element *element : found)
         element->SetClass("disabled", arrow.dead);
   }
}

extern "C" int rib_rmlui_turn_list_page(int delta)
{
   Rml::Element *list = rib_visible_list();
   if (!list)
      return -1;
   std::vector<Rml::Element*> pages;
   rib_collect(list, "list-page", pages);
   std::vector<Rml::Element*> usable;
   for (Rml::Element *page : pages)
      if (rib_page_has_row(page))
         usable.push_back(page);
   if (usable.size() < 2)
      return -1;
   int current = 0;
   for (size_t index = 0; index < usable.size(); ++index)
      if (!rib_display_none(usable[index]))
         current = (int)index;
   int next = current + (delta < 0 ? -1 : 1);
   if (next < 0 || next >= (int)usable.size())
      return -1;
   for (size_t index = 0; index < usable.size(); ++index)
   {
      if ((int)index == next)
         usable[index]->RemoveProperty("display");
      else
         usable[index]->SetProperty("display", "none");
   }
   std::vector<Rml::Element*> counts;
   rib_collect(list, "list-pager-count", counts);
   if (!counts.empty())
   {
      char label[32];
      snprintf(label, sizeof(label), "%d/%d", next + 1, (int)usable.size());
      counts[0]->SetInnerRML(label);
   }
   rib_mark_pager(list, next, (int)usable.size());
   return next;
}

extern "C" void rib_rmlui_mark_row(const char *id, const char *on, const char *off)
{
   Rml::Element *list = rib_visible_list();
   if (!document || !list)
      return;
   std::vector<Rml::Element*> rows;
   rib_collect(list, "list-row", rows);
   for (Rml::Element *row : rows)
   {
      const bool selected = id && row->GetId() == id;
      row->SetClass("selected", selected);
      if (Rml::Element *state = document->GetElementById(row->GetId() + "-state"))
         state->SetInnerRML(Rml::StringUtilities::EncodeRml(
               selected ? (on ? on : "") : (off ? off : "")));
   }
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
    * to the button here and not during the load. A screen may list several
    * buttons, separated by spaces. The BACK button of a list opens the screen
    * it returns to, so we add that button to the declaration from the host. */
   if (document && button && *button)
   {
      const char *cursor = button;
      while (*cursor)
      {
         while (*cursor == ' ')
            ++cursor;
         const char *end = cursor;
         while (*end && *end != ' ')
            ++end;
         if (end > cursor)
         {
            std::string one(cursor, end);
            if (wired_screen_buttons.insert(one).second)
               if (Rml::Element *element = document->GetElementById(one))
                  element->AddEventListener(Rml::EventId::Click,
                        new ScreenListener(id));
         }
         cursor = end;
      }
   }
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

extern "C" void rib_rmlui_set_overlay_mode(bool only_overlays)
{
   if (only_overlays)
      rib_rmlui_clear_intents();
   if (!document)
      return;
   if (Rml::Element *body = document->GetElementById("body"))
      body->SetClass("overlay", only_overlays);
}

extern "C" void rib_rmlui_set_overlay(const char *element,
      enum rib_overlay_state state)
{
   if (!document || !element || !*element)
      return;
   if (Rml::Element *overlay = document->GetElementById(element))
   {
      overlay->SetClass("showing", state == RIB_OVERLAY_SHOWING);
      overlay->SetClass("leaving", state == RIB_OVERLAY_LEAVING);
   }
}

/* A slider or a toggle, found by its class in the design. We never describe
 * the markup in the control, and only ask for the part. */
static Rml::Element *find_class(Rml::Element *node, const char *cls)
{
   if (!node)
      return nullptr;
   if (node->IsClassSet(cls))
      return node;
   const int count = node->GetNumChildren();
   for (int index = 0; index < count; ++index)
      if (Rml::Element *found = find_class(node->GetChild(index), cls))
         return found;
   return nullptr;
}

static Rml::Element *slider_ancestor(Rml::Element *node)
{
   for (; node; node = node->GetParentNode())
      if (node->IsClassSet("slider"))
         return node;
   return nullptr;
}

static std::map<std::string, float> slider_fraction;
static std::map<std::string, float> slider_step;
static std::string slider_drag_id;
static float slider_drag_fraction = 0.0f;
static float slider_drag_origin = 0.0f;
static std::string changed_part;
static float changed_fraction = 0.0f;
static bool changed_on = false;
static int pointer_x = 0;
static int pointer_y = 0;

static float clamp_fraction(float fraction)
{
   if (fraction < 0.0f)
      return 0.0f;
   if (fraction > 1.0f)
      return 1.0f;
   return fraction;
}

static float fraction_at(Rml::Element *slider, int x)
{
   Rml::Element *track = find_class(slider, "slider-track");
   if (!track)
      return 0.0f;
   if (context)
      context->Update();
   const float left = track->GetAbsoluteOffset(Rml::BoxArea::Border).x;
   const float width = track->GetBox().GetSize(Rml::BoxArea::Border).x;
   if (width <= 1.0f)
      return 0.0f;
   return clamp_fraction((static_cast<float>(x) - left) / width);
}

static void paint_slider(Rml::Element *slider, float fraction, const char *readout)
{
   if (!slider)
      return;
   fraction = clamp_fraction(fraction);
   slider_fraction[slider->GetId()] = fraction;
   if (context)
      context->Update();
   Rml::Element *track = find_class(slider, "slider-track");
   Rml::Element *fill = find_class(slider, "slider-fill");
   Rml::Element *thumb = find_class(slider, "slider-thumb");
   const float width = track
         ? track->GetBox().GetSize(Rml::BoxArea::Content).x : 0.0f;
   const float thumb_width = thumb
         ? thumb->GetBox().GetSize(Rml::BoxArea::Border).x : 0.0f;
   if (fill && width > 0.0f)
      fill->SetProperty("width", std::to_string(width * fraction) + "px");
   if (thumb && width > 0.0f)
   {
      const float travel = std::max(0.0f, width - thumb_width);
      thumb->SetProperty("left", std::to_string(travel * fraction) + "px");
   }
   if (readout)
      if (Rml::Element *text = find_class(slider, "slider-readout"))
         text->SetInnerRML(Rml::StringUtilities::EncodeRml(readout));
}

static void remember_slider(const std::string &id, float fraction)
{
   changed_part = id;
   changed_fraction = clamp_fraction(fraction);
   ActionListener::queue_action(RIB_RMLUI_ACTION_SLIDER);
}

/* One move cue when the level changes, and none when it does not. At an
 * end, we clamp a further step to the same fraction, which is not a move, so
 * we play the cue only when the volume changes. */
static void note_slider_move(float before, float after)
{
   const float delta = after - before;
   if (delta > 0.0001f || delta < -0.0001f)
      rib_rmlui_play_move_sound(delta > 0.0f ? 1 : -1);
}

class PartToggleListener : public Rml::EventListener
{
public:
   explicit PartToggleListener(std::string id) : id(std::move(id)) {}
   void ProcessEvent(Rml::Event &event) override
   {
      Rml::Element *element = event.GetCurrentElement();
      if (!element || element->HasAttribute("disabled")
            || element->IsClassSet("disabled"))
         return;
      const bool on = !element->IsClassSet("on");
      element->SetClass("on", on);
      changed_part = id;
      changed_on = on;
      /* Not a list switch. Leaving the last list id set would flip that
       * switch when this part is clicked. */
      chosen_item.clear();
      ActionListener::queue_action(RIB_RMLUI_ACTION_TOGGLE);
   }
   void OnDetach(Rml::Element *) override { delete this; }
private:
   std::string id;
};

static void wire_part_toggles(Rml::Element *node)
{
   if (!node)
      return;
   if (node->IsClassSet("toggle") && !node->GetId().empty())
      node->AddEventListener(Rml::EventId::Click, new PartToggleListener(node->GetId()));
   const int count = node->GetNumChildren();
   for (int index = 0; index < count; ++index)
      wire_part_toggles(node->GetChild(index));
}

/* An arrow next to a slider. A click on it moves the slider by the step of
 * that slider, the same change as with a key. Direction is a class because it
 * is one of two, not a number written into the markup. */
class ArrowListener : public Rml::EventListener
{
public:
   ArrowListener(std::string slider, int direction)
      : slider(std::move(slider)), direction(direction) {}
   void ProcessEvent(Rml::Event &event) override
   {
      Rml::Element *element = event.GetCurrentElement();
      if (!element || element->HasAttribute("disabled")
            || element->IsClassSet("disabled") || direction == 0)
         return;
      rib_rmlui_nudge_slider(slider.c_str(), direction);
   }
   void OnDetach(Rml::Element *) override { delete this; }
private:
   std::string slider;
   int direction;
};

static Rml::Element *find_slider(Rml::Element *node)
{
   if (!node)
      return nullptr;
   if (node->IsClassSet("slider") && !node->GetId().empty())
      return node;
   const int count = node->GetNumChildren();
   for (int index = 0; index < count; ++index)
      if (Rml::Element *found = find_slider(node->GetChild(index)))
         return found;
   return nullptr;
}

static void wire_arrows(Rml::Element *node)
{
   if (!node)
      return;
   if (node->IsClassSet("volume-arrow"))
   {
      const int direction = node->IsClassSet("arrow-down") ? -1
            : node->IsClassSet("arrow-up") ? 1 : 0;
      Rml::Element *slider = find_slider(node->GetParentNode());
      if (slider && direction != 0)
         node->AddEventListener(Rml::EventId::Click,
               new ArrowListener(slider->GetId(), direction));
   }
   const int count = node->GetNumChildren();
   for (int index = 0; index < count; ++index)
      wire_arrows(node->GetChild(index));
}

extern "C" const char *rib_rmlui_changed_part(void)
{
   return changed_part.c_str();
}

extern "C" float rib_rmlui_changed_fraction(void)
{
   return changed_fraction;
}

extern "C" bool rib_rmlui_changed_on(void)
{
   return changed_on;
}

extern "C" const char *rib_rmlui_screen_panel(const char *id)
{
   if (!id)
      return "";
   for (const Screen &screen : screens)
      if (screen.id == id)
         return screen.panel.c_str();
   return "";
}

extern "C" void rib_rmlui_set_slider(const char *id, float fraction, const char *readout)
{
   if (!document || !id)
      return;
   if (Rml::Element *slider = document->GetElementById(id))
      if (slider->IsClassSet("slider"))
         paint_slider(slider, fraction, readout);
}

extern "C" bool rib_rmlui_commit_slider(const char *id, float fraction);

extern "C" void rib_rmlui_set_slider_step(const char *id, float step)
{
   if (id && *id && step > 0.0f)
      slider_step[id] = step;
}

extern "C" bool rib_rmlui_nudge_slider(const char *id, int direction)
{
   if (!id || direction == 0)
      return false;
   const auto step = slider_step.find(id);
   if (step == slider_step.end())
      return false;
   float current = 0.0f;
   const auto found = slider_fraction.find(id);
   if (found != slider_fraction.end())
      current = found->second;
   return rib_rmlui_commit_slider(id, current + (float)direction * step->second);
}

extern "C" bool rib_rmlui_commit_slider(const char *id, float fraction)
{
   if (!document || !id)
      return false;
   Rml::Element *slider = document->GetElementById(id);
   if (!slider || !slider->IsClassSet("slider"))
      return false;
   float before = 0.0f;
   const auto found = slider_fraction.find(slider->GetId());
   if (found != slider_fraction.end())
      before = found->second;
   const float after = clamp_fraction(fraction);
   paint_slider(slider, fraction, nullptr);
   remember_slider(slider->GetId(), fraction);
   note_slider_move(before, after);
   return true;
}

extern "C" bool rib_rmlui_slider_drag(const char **id, float *fraction)
{
   if (!slider_drag)
      return false;
   if (id)
      *id = slider_drag_id.c_str();
   if (fraction)
      *fraction = slider_drag_fraction;
   return true;
}

static void drag_to(int x)
{
   if (!slider_drag)
      return;
   slider_drag_fraction = fraction_at(slider_drag, x);
   paint_slider(slider_drag, slider_drag_fraction, nullptr);
}

static void end_drag(void)
{
   if (!slider_drag)
      return;
   slider_drag->SetClass("dragging", false);
   const float after = clamp_fraction(slider_drag_fraction);
   remember_slider(slider_drag_id, slider_drag_fraction);
   note_slider_move(slider_drag_origin, after);
   slider_drag = nullptr;
}

static void collect_focusable(Rml::Element *node, std::vector<std::string> &out)
{
   if (!node)
      return;
   /* We keep a hidden entry in the document, because we cannot create it in
    * the player after the core reports how many discs it loaded. It must not
    * be a stop, or moving down from CONTROLS would reach DISC and show nothing.
    * display:none alone is not enough, because in the pause walk we skip only
    * disabled entries. In this walk we skip both, and we also disable the
    * entry. */
   if (rib_display_none(node) || node->HasAttribute("disabled")
         || node->IsClassSet("disabled"))
      return;
   /* The arrows next to a slider are for stepping it with a pointer. Left
    * and right on the slider already step it, so an arrow is not a stop, and
    * moving down from the slider does not reach the arrows. */
   const bool affordance = node->IsClassSet("volume-arrow");
   const bool part = !affordance && (node->IsClassSet("slider")
         || node->IsClassSet("toggle") || node->IsClassSet("menu-action"));
   if (part && !node->GetId().empty())
      out.push_back(node->GetId());
   const int count = node->GetNumChildren();
   for (int index = 0; index < count; ++index)
      collect_focusable(node->GetChild(index), out);
}

extern "C" int rib_rmlui_focusables(const char *panel, char ids[][64], int capacity)
{
   if (!document || !panel || !ids || capacity <= 0)
      return 0;
   Rml::Element *root = document->GetElementById(panel);
   std::vector<std::string> found;
   collect_focusable(root, found);
   int count = 0;
   for (const std::string &id : found)
   {
      if (count >= capacity)
         break;
      std::snprintf(ids[count], 64, "%s", id.c_str());
      ++count;
   }
   return count;
}

extern "C" void rib_rmlui_mark_focused(const char *panel, const char *id)
{
   char ids[16][64];
   const int count = rib_rmlui_focusables(panel, ids, 16);
   for (int index = 0; index < count; ++index)
      if (Rml::Element *element = document->GetElementById(ids[index]))
         element->SetClass("focused", id && std::strcmp(ids[index], id) == 0);
}

extern "C" bool rib_rmlui_part_is_slider(const char *id)
{
   if (!document || !id)
      return false;
   Rml::Element *element = document->GetElementById(id);
   return element && element->IsClassSet("slider");
}

extern "C" void rib_rmlui_pointer_move(int x, int y)
{
   pointer_x = x;
   pointer_y = y;
   if (context)
      context->ProcessMouseMove(x, y, 0);
   if (slider_drag)
      drag_to(x);
}

extern "C" void rib_rmlui_pointer_button(bool down)
{
   if (!context)
      return;
   if (down == pointer_down)
      return;
   pointer_down = down;
   if (down)
   {
      context->ProcessMouseButtonDown(0, 0);
      if (Rml::Element *slider = slider_ancestor(context->GetHoverElement()))
      {
         slider_drag = slider;
         slider_drag_id = slider->GetId();
         slider_drag_origin = 0.0f;
         const auto painted = slider_fraction.find(slider->GetId());
         if (painted != slider_fraction.end())
            slider_drag_origin = painted->second;
         slider->SetClass("dragging", true);
         drag_to(pointer_x);
      }
   }
   else
   {
      if (slider_drag)
         drag_to(pointer_x);
      context->ProcessMouseButtonUp(0, 0);
      end_drag();
   }
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
   if (slider_drag)
      end_drag();
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

extern "C" bool rib_rmlui_element_box(const char *id, int *x, int *y, int *w, int *h)
{
   if (!context || !document || !id || !x || !y || !w || !h)
      return false;
   context->Update();
   Rml::Element *element = document->GetElementById(id);
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

extern "C" int rib_rmlui_hovered_list_row(void)
{
   if (!context || !document)
      return -1;
   Rml::Element *cursor = context->GetHoverElement();
   Rml::Element *row = nullptr;
   for (; cursor; cursor = cursor->GetParentNode())
   {
      if (cursor->IsClassSet("list-row"))
      {
         row = cursor;
         break;
      }
   }
   if (!row)
      return -1;
   std::vector<Rml::Element*> rows;
   rib_visible_rows(rows);
   for (int index = 0; index < (int)rows.size(); ++index)
      if (rows[index] == row)
         return index;
   return -1;
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

static void rib_set_text(const std::string& id, const char *text)
{
   if (!document)
      return;
   if (Rml::Element *element = document->GetElementById(id))
      element->SetInnerRML(Rml::StringUtilities::EncodeRml(text ? text : ""));
}

extern "C" void rib_rmlui_set_element_text(const char *id, const char *text)
{
   if (!id || !*id)
      return;
   rib_set_text(id, text);
}

extern "C" void rib_rmlui_set_row_text(const char *id, const char *title,
      const char *detail, const char *state)
{
   if (!id || !*id)
      return;
   const std::string row(id);
   rib_set_text(row + "-title", title);
   rib_set_text(row + "-detail", detail);
   rib_set_text(row + "-state", state);
}

extern "C" void rib_rmlui_set_shown(const char *id, bool shown)
{
   if (!document || !id)
      return;
   if (Rml::Element *element = document->GetElementById(id))
   {
      if (shown)
         element->RemoveProperty("display");
      else
         element->SetProperty("display", "none");
   }
}

extern "C" void rib_rmlui_set_disabled(const char *id, bool disabled)
{
   if (!document || !id)
      return;
   Rml::Element *element = document->GetElementById(id);
   if (!element)
      return;
   element->SetClass("disabled", disabled);
   if (disabled)
      element->SetAttribute("disabled", "disabled");
   else
      element->RemoveAttribute("disabled");
}

static Rml::Element *rib_list_element(const char *list_id);

static float rib_specified_dp(Rml::Element *element, const char *name);

static int rib_utf8_len(unsigned char lead)
{
   if ((lead & 0x80) == 0)
      return 1;
   if ((lead & 0xe0) == 0xc0)
      return 2;
   if ((lead & 0xf0) == 0xe0)
      return 3;
   if ((lead & 0xf8) == 0xf0)
      return 4;
   return 1;
}

static int rib_chars(const std::string &text)
{
   int count = 0;
   for (size_t index = 0; index < text.size(); )
   {
      int len = rib_utf8_len((unsigned char)text[index]);
      if (index + (size_t)len > text.size())
         len = 1;
      index += (size_t)len;
      ++count;
   }
   return count;
}

static std::string rib_slice(const std::string &text, int from, int count)
{
   std::string out;
   int seen = 0;
   for (size_t index = 0; index < text.size() && seen < from + count; )
   {
      int len = rib_utf8_len((unsigned char)text[index]);
      if (index + (size_t)len > text.size())
         len = 1;
      if (seen >= from)
         out.append(text, index, (size_t)len);
      index += (size_t)len;
      ++seen;
   }
   return out;
}

/* The first ancestor with a width in dp. A row is often 100%, which is not a
 * length, so we measure the title against the width of the list. From it we
 * get the column. With a copied pixel width, the preview and the game could
 * differ. */
static float rib_block_dp(Rml::Element *element)
{
   for (Rml::Element *cursor = element; cursor; cursor = cursor->GetParentNode())
   {
      const float width = rib_specified_dp(cursor, "width");
      if (width > 0.f)
         return width;
   }
   return 0.f;
}

extern "C" void rib_rmlui_fit_row_title(const char *id, const char *text)
{
   if (!id || !*id)
      return;
   const std::string title_id = std::string(id) + "-title";
   const std::string source = text ? text : "";
   Rml::Element *element = document ? document->GetElementById(title_id) : nullptr;
   if (!element || !context)
   {
      rib_set_text(title_id, source.c_str());
      return;
   }
   context->Update();
   const float density = std::max(context->GetDensityIndependentPixelRatio(), 0.1f);
   const float block = rib_block_dp(element);
   const float limit_dp = block
         - rib_specified_dp(element, "left")
         - rib_specified_dp(element, "right");
   const float limit_px = limit_dp * density;
   auto width_of = [&](const std::string &value) {
      return (float)Rml::ElementUtilities::GetStringWidth(
            element, Rml::String(value));
   };
   if (limit_px <= 1.f || width_of(source) <= limit_px)
   {
      rib_set_text(title_id, source.c_str());
      return;
   }
   /* text-overflow in RmlUi removes characters at the end, where the disc
    * number is. We put the mark in the middle instead, and measure the width
    * with the font of this element, not by counting characters, because the
    * Silkscreen advances are not all equal. */
   const std::string mark = "\u2026";
   const int total = rib_chars(source);
   int best = 0;
   int low = 0;
   int high = total;
   while (low <= high)
   {
      const int mid = (low + high) / 2;
      const int head = mid / 2;
      const int tail = mid - head;
      const std::string candidate = rib_slice(source, 0, head) + mark
            + rib_slice(source, total - tail, tail);
      if (width_of(candidate) <= limit_px)
      {
         best = mid;
         low = mid + 1;
      }
      else
         high = mid - 1;
   }
   const int head = best / 2;
   const int tail = best - head;
   rib_set_text(title_id, (rib_slice(source, 0, head) + mark
         + rib_slice(source, total - tail, tail)).c_str());
}

extern "C" void rib_rmlui_select_row(const char *list_id, const char *row_id,
      const char *on, const char *off)
{
   if (!document || !list_id)
      return;
   std::vector<Rml::Element*> rows;
   rib_collect(rib_list_element(list_id), "list-row", rows);
   for (Rml::Element *row : rows)
   {
      const bool selected = row_id && row->GetId() == row_id;
      row->SetClass("selected", selected);
      if (Rml::Element *state = document->GetElementById(row->GetId() + "-state"))
         state->SetInnerRML(Rml::StringUtilities::EncodeRml(
               selected ? (on ? on : "") : (off ? off : "")));
   }
}

static Rml::Element *rib_list_element(const char *list_id)
{
   if (!document || !list_id || !*list_id)
      return nullptr;
   return document->GetElementById(list_id);
}

extern "C" int rib_rmlui_rows_in(const char *list_id)
{
   std::vector<Rml::Element*> rows;
   rib_collect(rib_list_element(list_id), "list-row", rows);
   return (int)rows.size();
}

extern "C" const char *rib_rmlui_row_in(const char *list_id, int index)
{
   static std::string id;
   std::vector<Rml::Element*> rows;
   rib_collect(rib_list_element(list_id), "list-row", rows);
   id.clear();
   if (index >= 0 && index < (int)rows.size())
      id = rows[index]->GetId();
   return id.c_str();
}

extern "C" void rib_rmlui_retarget_pages(const char *list_id)
{
   Rml::Element *list = rib_list_element(list_id);
   if (!list)
      return;
   std::vector<Rml::Element*> pages;
   rib_collect(list, "list-page", pages);
   std::vector<Rml::Element*> usable;
   for (Rml::Element *page : pages)
   {
      if (rib_page_has_row(page))
         usable.push_back(page);
      else
         page->SetProperty("display", "none");
   }
   for (size_t index = 0; index < usable.size(); ++index)
   {
      if (index == 0)
         usable[index]->RemoveProperty("display");
      else
         usable[index]->SetProperty("display", "none");
   }
   std::vector<Rml::Element*> pagers;
   rib_collect(list, "list-pager", pagers);
   if (pagers.empty())
      return;
   if (usable.size() < 2)
   {
      pagers[0]->SetProperty("display", "none");
      return;
   }
   pagers[0]->RemoveProperty("display");
   std::vector<Rml::Element*> counts;
   rib_collect(list, "list-pager-count", counts);
   if (!counts.empty())
   {
      char label[32];
      snprintf(label, sizeof(label), "1/%d", (int)usable.size());
      counts[0]->SetInnerRML(label);
   }
   rib_mark_pager(list, 0, (int)usable.size());
}

/* A length in dp or px from the stylesheet, as dp. Keywords (auto) are 0,
 * because they are not a gap to leave free for the text. */
static float rib_specified_dp(Rml::Element *element, const char *name)
{
   const Rml::Property *property = element ? element->GetProperty(name) : nullptr;
   if (!property)
      return 0.f;
   if (property->unit != Rml::Unit::DP && property->unit != Rml::Unit::PX
         && property->unit != Rml::Unit::NUMBER)
      return 0.f;
   const float value = property->value.Get<float>();
   if (property->unit != Rml::Unit::PX || !context)
      return value;
   const float density = std::max(
         context->GetDensityIndependentPixelRatio(), 0.1f);
   return value / density;
}

/* The width required for the widest row, in dp, at most the width in the
 * design.
 *
 * The three parts of a row are absolutely positioned, so the box has no
 * natural width. The `right` offset of the title is not the detail slot.
 * The title stops 120dp from the edge and the detail is 128dp wide, so a
 * size from that offset would draw the title on top of the detail.
 * The width is the words of the title, a gap, and the fixed slots. While
 * the pager is open, the width is at least that of its buttons. */
static int rib_list_width(Rml::Element *list, int declared)
{
   static const char *const parts[] = {"list-row-title", "list-row-detail",
         "list-row-state"};
   const float density = std::max(context
         ? context->GetDensityIndependentPixelRatio() : 1.f, 0.1f);
   float widest = 0.f;
   std::vector<Rml::Element*> rows;

   rib_collect(list, "list-row", rows);
   for (Rml::Element *row : rows)
   {
      if (rib_hidden(row))
         continue;
      float glyphs = 0.f;
      float reserved = 0.f;
      for (const char *part : parts)
      {
         std::vector<Rml::Element*> found;
         rib_collect(row, part, found);
         for (Rml::Element *element : found)
         {
            if (rib_display_none(element))
               continue;
            const Rml::String text = element->GetInnerRML();
            if (text.empty())
               continue;
            const float words = Rml::ElementUtilities::GetStringWidth(
                  element, text) / density;
            const float left = rib_specified_dp(element, "left");
            const float right = rib_specified_dp(element, "right");
            const float width = rib_specified_dp(element, "width");
            const Rml::Property *left_prop = element->GetProperty("left");
            const Rml::Property *right_prop = element->GetProperty("right");
            const bool left_set = left_prop
                  && left_prop->unit != Rml::Unit::KEYWORD;
            const bool right_set = right_prop
                  && right_prop->unit != Rml::Unit::KEYWORD;
            if (left_set && right_set && width <= 0.f)
               glyphs = std::max(glyphs, left + words);
            else if (width > 0.f && right_set)
               reserved = std::max(reserved, right + std::max(width, words));
            else
               glyphs = std::max(glyphs, words);
         }
      }
      /* 8dp between the title's last letter and the detail's slot. Without
       * it the two strings run together, as in "ABUTTON". */
      const float need = glyphs + reserved + (glyphs > 0.f && reserved > 0.f ? 8.f : 0.f);
      if (need > widest)
         widest = need;
   }

   std::vector<Rml::Element*> pagers;
   rib_collect(list, "list-pager", pagers);
   if (!pagers.empty() && !rib_display_none(pagers[0]) && !rib_hidden(pagers[0]))
   {
      for (const char *cls : {"list-pager-prev", "list-pager-count",
            "list-pager-next"})
      {
         std::vector<Rml::Element*> found;
         rib_collect(pagers[0], cls, found);
         for (Rml::Element *button : found)
         {
            const float edge = rib_specified_dp(button, "left")
                  + rib_specified_dp(button, "width");
            if (edge > widest)
               widest = edge;
         }
      }
   }

   if (widest <= 0.f)
      return declared;
   const int dp = (int)std::ceil(widest);
   return dp < declared ? dp : declared;
}

/* How far a visible child is drawn outside the border box of the list. A
 * row is 100% wide plus its border, so it extends past the box. With a
 * clamp that measured only the list, the row would end on the last pixel. */
static void rib_paint_overflow(Rml::Element *list,
      float &extra_left, float &extra_top, float &extra_right, float &extra_bottom)
{
   extra_left = extra_top = extra_right = extra_bottom = 0.f;
   const Rml::Vector2f list_at = list->GetAbsoluteOffset(Rml::BoxArea::Border);
   const Rml::Vector2f list_size = list->GetBox().GetSize(Rml::BoxArea::Border);
   const char *classes[] = {"list-row", "list-pager"};
   for (const char *cls : classes)
   {
      std::vector<Rml::Element*> found;
      rib_collect(list, cls, found);
      for (Rml::Element *child : found)
      {
         if (rib_hidden(child))
            continue;
         const Rml::Vector2f at = child->GetAbsoluteOffset(Rml::BoxArea::Border);
         const Rml::Vector2f size = child->GetBox().GetSize(Rml::BoxArea::Border);
         extra_left = std::max(extra_left, list_at.x - at.x);
         extra_top = std::max(extra_top, list_at.y - at.y);
         extra_right = std::max(extra_right,
               at.x + size.x - (list_at.x + list_size.x));
         extra_bottom = std::max(extra_bottom,
               at.y + size.y - (list_at.y + list_size.y));
      }
   }
}

/* `left`/`top` are added to the padding edge of the offset parent (and to the
 * margin of the element), so we store in the clamp a distance from that
 * padding edge. From the border edge of the screen, the list would be one
 * border width too far right, and in a 2x window its border would be on the
 * last pixel of the window. */
static void rib_set_border_position(Rml::Element *list, float abs_x, float abs_y)
{
   float origin_x = 0.f;
   float origin_y = 0.f;
   if (Rml::Element *parent = list->GetOffsetParent())
   {
      const Rml::Vector2f padding =
            parent->GetAbsoluteOffset(Rml::BoxArea::Padding);
      origin_x = padding.x;
      origin_y = padding.y;
   }
   const float margin_x = list->GetBox().GetEdge(
         Rml::BoxArea::Margin, Rml::BoxEdge::Left);
   const float margin_y = list->GetBox().GetEdge(
         Rml::BoxArea::Margin, Rml::BoxEdge::Top);
   list->SetProperty("left", std::to_string(
         (int)std::lround(abs_x - origin_x - margin_x)) + "px");
   list->SetProperty("top", std::to_string(
         (int)std::lround(abs_y - origin_y - margin_y)) + "px");
}

static void rib_clamp_border(float &x, float &y, float w, float h,
      const Rml::Vector2f &screen_at, const Rml::Vector2f &screen_size)
{
   const float margin = 8.f;
   const float min_x = screen_at.x + margin;
   const float min_y = screen_at.y + margin;
   const float max_x = screen_at.x + screen_size.x - margin;
   const float max_y = screen_at.y + screen_size.y - margin;
   if (x + w > max_x)
      x = max_x - w;
   if (x < min_x)
      x = min_x;
   if (y + h > max_y)
      y = max_y - h;
   if (y < min_y)
      y = min_y;
}

/* What a list must not cover.
 *
 * A callout or a stick group is a label, except the one the list is for.
 * The player is reading that box, so it counts as chrome, which the list
 * may never cover. The leader line and the hit ring are not in this table.
 * The heading, the label and button of the controller picker, the buttons
 * of the screen, the status line and the footer are chrome too. The option
 * list of the picker is display:none while a bind list is open, and we
 * skip the pager, a menu-action inside the list. */
struct rib_keep_clear
{
   const char *name;
   bool id;
   bool label;
};

static const rib_keep_clear rib_keep_clear_rules[] = {
   {"control-callout", false, true},
   {"control-group", false, true},
   {"menu-action", false, false},
   {"heading", true, false},
   {"control-picker-label", false, false},
   {"control-picker-current", false, false},
   {"controls-status", true, false},
   {"footer", true, false},
};

static bool rib_under(Rml::Element *ancestor, Rml::Element *element)
{
   for (Rml::Element *node = element; node; node = node->GetParentNode())
      if (node == ancestor)
         return true;
   return false;
}

static void rib_count_covered(Rml::Element *list, const char *anchor_id,
      float left, float top, float width, float height,
      int &labels, int &chrome)
{
   labels = 0;
   chrome = 0;
   std::set<Rml::String> seen;
   for (const rib_keep_clear &rule : rib_keep_clear_rules)
   {
      std::vector<Rml::Element*> found;
      if (rule.id)
      {
         if (Rml::Element *element = document->GetElementById(rule.name))
            found.push_back(element);
      }
      else
         rib_collect(document, rule.name, found);
      for (Rml::Element *element : found)
      {
         if (!element || rib_hidden(element) || (list && rib_under(list, element)))
            continue;
         const Rml::String id = element->GetId();
         if (id.empty() || !seen.insert(id).second)
            continue;
         const Rml::Vector2f at = element->GetAbsoluteOffset(Rml::BoxArea::Border);
         const Rml::Vector2f size = element->GetBox().GetSize(Rml::BoxArea::Border);
         if (at.x < left + width && at.x + size.x > left
               && at.y < top + height && at.y + size.y > top)
         {
            /* The anchor matches a label rule. Count it as chrome, so we
             * prefer a spot that leaves the control being read uncovered. */
            if (rule.label && !(anchor_id && id == anchor_id))
               ++labels;
            else
               ++chrome;
         }
      }
   }
}

extern "C" void rib_rmlui_place_list(const char *list_id, const char *anchor_id,
      int width_dp)
{
   Rml::Element *list = rib_list_element(list_id);
   Rml::Element *anchor = document && anchor_id ? document->GetElementById(anchor_id) : nullptr;
   Rml::Element *screen = document ? document->GetElementById("screen") : nullptr;
   if (!list || !context)
      return;
   list->RemoveProperty("display");
   if (width_dp > 0)
      list->SetProperty("width", std::to_string(rib_list_width(list, width_dp)) + "dp");
   if (!anchor || !screen)
      return;
   context->Update();
   const Rml::Vector2f screen_at = screen->GetAbsoluteOffset(Rml::BoxArea::Border);
   const Rml::Vector2f anchor_at = anchor->GetAbsoluteOffset(Rml::BoxArea::Border);
   const Rml::Vector2f anchor_size = anchor->GetBox().GetSize(Rml::BoxArea::Border);
   const Rml::Vector2f list_size = list->GetBox().GetSize(Rml::BoxArea::Border);
   const Rml::Vector2f screen_size = screen->GetBox().GetSize(Rml::BoxArea::Border);
   float extra_left = 0.f, extra_top = 0.f, extra_right = 0.f, extra_bottom = 0.f;
   rib_paint_overflow(list, extra_left, extra_top, extra_right, extra_bottom);
   const float paint_w = list_size.x + extra_left + extra_right;
   const float paint_h = list_size.y + extra_top + extra_bottom;
   const float gap = 4.f;

   /* Beside the label, toward the pad (over the drawing), then below, then
    * above. Covering the drawing is fine. Covering another label is not. */
   float scene_cx = anchor_at.x + anchor_size.x * 0.5f;
   if (Rml::Element *scene = document->GetElementById("controller-scene"))
   {
      const Rml::Vector2f scene_at = scene->GetAbsoluteOffset(Rml::BoxArea::Border);
      const Rml::Vector2f scene_size = scene->GetBox().GetSize(Rml::BoxArea::Border);
      scene_cx = scene_at.x + scene_size.x * 0.5f;
   }
   const float anchor_cx = anchor_at.x + anchor_size.x * 0.5f;
   const float beside_x = scene_cx >= anchor_cx
         ? anchor_at.x + anchor_size.x + gap
         : anchor_at.x - list_size.x - gap;
   const float away_x = scene_cx >= anchor_cx
         ? anchor_at.x - list_size.x - gap
         : anchor_at.x + anchor_size.x + gap;
   /* Clamping can move the spots beside and away back onto the anchor. We
    * may cover the drawing, so once each of the four sides covers something,
    * we also try the middle of the pad and the screen margins. */
   std::vector<float> columns = {
      beside_x,
      anchor_at.x,
      away_x,
      screen_at.x + 8.f,
      screen_at.x + screen_size.x - list_size.x - 8.f,
   };
   if (Rml::Element *scene = document->GetElementById("controller-scene"))
   {
      const Rml::Vector2f scene_at = scene->GetAbsoluteOffset(Rml::BoxArea::Border);
      const Rml::Vector2f scene_size = scene->GetBox().GetSize(Rml::BoxArea::Border);
      columns.push_back(scene_at.x + gap);
      columns.push_back(scene_at.x + (scene_size.x - list_size.x) * 0.5f);
      columns.push_back(scene_at.x + scene_size.x - list_size.x - gap);
   }
   std::vector<std::pair<float, float>> spots = {
      {beside_x, anchor_at.y},
      {anchor_at.x, anchor_at.y + anchor_size.y + gap},
      {anchor_at.x, anchor_at.y - list_size.y - gap},
      {away_x, anchor_at.y},
   };

   float best_x = anchor_at.x;
   float best_y = anchor_at.y + anchor_size.y + gap;
   int best_labels = 1000000;
   int best_chrome = 1000000;
   bool best_inside = false;
   bool have = false;
   bool settled = false;
   for (size_t index = 0; index < spots.size() && !settled; ++index)
   {
      float x = spots[index].first;
      float y = spots[index].second;
      rib_clamp_border(x, y, paint_w, paint_h, screen_at, screen_size);
      /* Clamping moves the border box of the list. Children that extend past
       * it must stay inside the same margin, so we test the painted area. */
      const float paint_x = x - extra_left;
      const float paint_y = y - extra_top;
      const bool inside =
            paint_x >= screen_at.x + 8.f - 0.5f
            && paint_y >= screen_at.y + 8.f - 0.5f
            && paint_x + paint_w <= screen_at.x + screen_size.x - 8.f + 0.5f
            && paint_y + paint_h <= screen_at.y + screen_size.y - 8.f + 0.5f;
      int labels = 0;
      int chrome = 0;
      rib_count_covered(list, anchor_id, paint_x, paint_y, paint_w, paint_h,
            labels, chrome);
      if (inside && chrome == 0 && labels == 0)
      {
         best_x = x;
         best_y = y;
         settled = true;
         break;
      }
      const bool better = !have
            || (inside && !best_inside)
            || (inside == best_inside && chrome < best_chrome)
            || (inside == best_inside && chrome == best_chrome && labels < best_labels);
      if (better)
      {
         best_x = x;
         best_y = y;
         best_labels = labels;
         best_chrome = chrome;
         best_inside = inside;
         have = true;
      }
      /* We try the three sides first. Only when each of them covers a button,
       * the status line or the footer do we look for a place clear of those.
       * When we count what a side covers, we still count only labels. */
      if (index == 3 && (best_chrome > 0 || best_labels > 0))
      {
         for (const rib_keep_clear &rule : rib_keep_clear_rules)
         {
            if (rule.label)
               continue;
            std::vector<Rml::Element*> found;
            if (rule.id)
            {
               if (Rml::Element *element = document->GetElementById(rule.name))
                  found.push_back(element);
            }
            else
               rib_collect(document, rule.name, found);
            for (Rml::Element *element : found)
            {
               if (!element || rib_hidden(element) || rib_under(list, element))
                  continue;
               const float top = element->GetAbsoluteOffset(Rml::BoxArea::Border).y;
               for (float column : columns)
                  spots.emplace_back(column, top - paint_h - gap);
            }
         }
         const float min_y = screen_at.y + 8.f;
         const float max_y = screen_at.y + screen_size.y - 8.f - paint_h;
         for (float scan_y = min_y; scan_y <= max_y; scan_y += 24.f)
            for (float column : columns)
               spots.emplace_back(column, scan_y);
      }
   }
   rib_set_border_position(list, best_x, best_y);
}

/* Relative to the screen. The labels are the callout and group elements in
 * the document. The box of a stick is control-group-l_stick, not
 * control-group- plus an axis id, so we would not find it in the control table. */
extern "C" int rib_rmlui_controls_covered(const char *anchor_id,
      int left, int top, int width, int height)
{
   if (!document || !context || width <= 0 || height <= 0)
      return 0;
   context->Update();
   Rml::Element *screen = document->GetElementById("screen");
   if (!screen)
      return 0;
   const Rml::Vector2f origin = screen->GetAbsoluteOffset(Rml::BoxArea::Border);
   int labels = 0;
   int chrome = 0;
   rib_count_covered(nullptr, anchor_id,
         origin.x + (float)left, origin.y + (float)top,
         (float)width, (float)height, labels, chrome);
   return labels;
}

extern "C" bool rib_rmlui_pointer_inside(const char *id, int x, int y)
{
   if (!document || !context || !id)
      return false;
   Rml::Element *element = document->GetElementById(id);
   if (!element || rib_hidden(element))
      return false;
   context->Update();
   const Rml::Vector2f offset = element->GetAbsoluteOffset(Rml::BoxArea::Border);
   const Rml::Vector2f size = element->GetBox().GetSize(Rml::BoxArea::Border);
   return x >= (int)offset.x && x < (int)(offset.x + size.x)
         && y >= (int)offset.y && y < (int)(offset.y + size.y);
}

extern "C" bool rib_rmlui_move_pointer_to(const char *id)
{
   int x = 0;
   int y = 0;
   if (!rib_rmlui_element_center(id, &x, &y))
      return false;
   rib_rmlui_pointer_move(x, y);
   return true;
}

extern "C" bool rib_rmlui_has_element(const char *id)
{
   return document && id && document->GetElementById(id);
}

extern "C" void rib_rmlui_focus_group(const char *group)
{
   if (!document)
      return;
   std::vector<Rml::Element*> groups;
   rib_collect(document, "control-group", groups);
   const std::string wanted = group && *group
         ? std::string("control-group-") + group : std::string();
   for (Rml::Element *element : groups)
      element->SetClass("focused", !wanted.empty() && element->GetId() == wanted);
}

#ifdef RIB_RMLUI_HEADLESS
/* The border box after layout, in window pixels. This is not the left and
 * width in the stylesheet, because we may clamp a list in one coordinate
 * space and draw it in another, and only from the box after layout can we tell which. */
extern "C" bool rib_rmlui_test_row_glyphs_overlap(const char *row_id)
{
   if (!document || !context || !row_id)
      return false;
   context->Update();
   Rml::Element *row = document->GetElementById(row_id);
   if (!row || rib_hidden(row))
      return false;
   auto span = [](Rml::Element *element, bool right_aligned,
         float &left, float &right) -> bool {
      if (!element || rib_display_none(element))
         return false;
      const Rml::String text = element->GetInnerRML();
      if (text.empty())
         return false;
      const float width = (float)Rml::ElementUtilities::GetStringWidth(element, text);
      if (width <= 0.f)
         return false;
      const Rml::Vector2f at = element->GetAbsoluteOffset(Rml::BoxArea::Padding);
      const float box = element->GetBox().GetSize(Rml::BoxArea::Content).x;
      if (right_aligned)
      {
         right = at.x + box;
         left = right - width;
      }
      else
      {
         left = at.x;
         right = left + width;
      }
      return true;
   };
   std::vector<Rml::Element*> titles;
   std::vector<Rml::Element*> details;
   rib_collect(row, "list-row-title", titles);
   rib_collect(row, "list-row-detail", details);
   if (titles.empty() || details.empty())
      return false;
   float title_left = 0.f, title_right = 0.f, detail_left = 0.f, detail_right = 0.f;
   if (!span(titles[0], false, title_left, title_right)
         || !span(details[0], true, detail_left, detail_right))
      return false;
   return title_right > detail_left + 0.5f;
}

extern "C" bool rib_rmlui_test_box(const char *id, int *x, int *y, int *w, int *h)
{
   if (!document || !context || !id || !x || !y || !w || !h)
      return false;
   context->Update();
   Rml::Element *element = document->GetElementById(id);
   if (!element || rib_hidden(element))
      return false;
   const Rml::Vector2f at = element->GetAbsoluteOffset(Rml::BoxArea::Border);
   const Rml::Vector2f size = element->GetBox().GetSize(Rml::BoxArea::Border);
   if (size.x <= 0.f || size.y <= 0.f)
      return false;
   *x = (int)std::floor(at.x);
   *y = (int)std::floor(at.y);
   *w = (int)std::ceil(at.x + size.x) - *x;
   *h = (int)std::ceil(at.y + size.y) - *y;
   return true;
}

/* Ids of the elements with a class, so in a check we walk the labels drawn in
 * the document and not a separate list. */
extern "C" int rib_rmlui_test_class_count(const char *class_name)
{
   std::vector<Rml::Element*> found;
   int count = 0;
   rib_collect(document, class_name, found);
   for (Rml::Element *element : found)
      if (!element->GetId().empty() && !rib_hidden(element))
         ++count;
   return count;
}

extern "C" const char *rib_rmlui_test_class_id(const char *class_name, int index)
{
   static std::string id;
   std::vector<Rml::Element*> found;
   int seen = 0;
   id.clear();
   rib_collect(document, class_name, found);
   for (Rml::Element *element : found)
   {
      if (element->GetId().empty() || rib_hidden(element))
         continue;
      if (seen == index)
      {
         id = element->GetId();
         return id.c_str();
      }
      ++seen;
   }
   return "";
}

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
/* A check for an element that is not there must fail, not crash. If we
 * dereferenced the lookup, one wrong id would end the whole run and hide
 * every check after it. */
extern "C" const char *rib_rmlui_test_text(const char *id) {
   static std::string text;
   Rml::Element *element = document ? document->GetElementById(id) : nullptr;
   text = element ? element->GetInnerRML() : std::string("<no element ") + id + ">";
   return text.c_str();
}
/* Which element has a class, so in a check we test what the player can see
 * and not the internal state of the driver. The focus record in the driver and
 * the classes in the document can differ, and we draw only the classes. */
extern "C" bool rib_rmlui_test_has_class(const char *id, const char *name) {
   Rml::Element *element = document && id ? document->GetElementById(id) : nullptr;
   return element && name && element->IsClassSet(name);
}
extern "C" float rib_rmlui_test_picture_aspect() {
   context->Update(); auto size = document->GetElementById("slot-image-1")->GetParentNode()->GetBox().GetSize(Rml::BoxArea::Content); return size.x / size.y;
}
#endif
