#include "rmlui_bridge.h"
#include "rmlui/elements.hpp"
#include "rmlui/document.hpp"
#include "rmlui/binds_popup.hpp"

#include <RmlUi/Core.h>
#include <RmlUi/Core/ElementUtilities.h>
#include <RmlUi/Core/StringUtilities.h>

#ifndef RIB_RMLUI_HEADLESS
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
rib::Document view;

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
   if (!view.root() || !id || !*id)
      return false;
   Rml::Element *element = view.root()->GetElementById(id);
   if (!element)
      return false;
   element->Click();
   return true;
}

void show_status(StatusMessage& message, const char *id, const char *text)
{
   message.text = text ? text : "";
   message.expires = view.elapsed() + 5.0;
   if (view.root())
      if (auto *element = view.root()->GetElementById(id))
         element->SetInnerRML(Rml::StringUtilities::EncodeRml(message.text));
}
void expire_status(StatusMessage& message, const char *id)
{
   if (!message.text.empty() && view.elapsed() >= message.expires)
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
bool pointer_down = false;

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
   if (!view.root())
      return;

   /* The contents of the pause panel, in document order. */
   char row[16][64];
   const int row_count = rib_rmlui_focusables("pause-panel", row, 16);
   for (int index = 0; index < row_count; ++index)
      if (Rml::Element *element = view.root()->GetElementById(row[index]))
         element->SetClass("focused", focused_element == row[index]);

   for (int index = 0; index < 6; ++index)
   {
      const int slot = index + 1;
      const std::string suffix = std::to_string(slot);
      if (Rml::Element *element = view.root()->GetElementById("slot-" + suffix))
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
      if (Rml::Element *label = view.root()->GetElementById("slot-label-" + suffix))
         label->SetInnerRML("SLOT " + suffix);
      if (Rml::Element *state = view.root()->GetElementById("slot-state-" + suffix))
         state->SetInnerRML(slots_guard.empty()
               ? (slots[index].occupied ? "OCCUPIED" : "EMPTY")
               : Rml::StringUtilities::EncodeRml(slots_guard));
      if (Rml::Element *image = view.root()->GetElementById("slot-image-" + suffix))
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
      if (Rml::Element *button = view.root()->GetElementById(id))
      {
         const bool disabled = !slots_guard.empty() ||
               (std::string(id) == "load" && !slots[selected_slot - 1].occupied);
         button->SetClass("disabled", disabled);
         if (disabled)
            button->SetAttribute("disabled", "disabled");
         else
            button->RemoveAttribute("disabled");
      }

   if (Rml::Element *status = view.root()->GetElementById("status"))
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
   if (!view.root())
      return;
   if (Rml::Element *current = view.root()->GetElementById("controls-device-current"))
      current->AddEventListener(Rml::EventId::Click,
            new ActionListener(RIB_RMLUI_ACTION_DEVICE_PICKER_TOGGLE));

   for (int index = 0; index < rib_rmlui_device_count(); ++index)
   {
      const char *id = rib_rmlui_device_id(index);
      if (!id || !*id)
         continue;
      if (Rml::Element *option =
            view.root()->GetElementById("controls-device-option-" + std::string(id)))
         option->AddEventListener(Rml::EventId::Click,
               new DeviceOptionListener(id));
   }
}

/* Show or hide the picker's list, and mark which option is in use. */
extern "C" void rib_rmlui_set_device_picker(bool open, const char *chosen)
{
   if (!view.root())
      return;
   if (Rml::Element *list = view.root()->GetElementById("controls-device-list"))
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
            view.root()->GetElementById("controls-device-option-" + std::string(id)))
         option->SetClass("selected", chosen && !std::strcmp(chosen, id));
   }
   if (Rml::Element *current = view.root()->GetElementById("controls-device-current"))
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
   if (!view.root())
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
         if (Rml::Element *element = view.root()->GetElementById(id))
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
      if (Rml::Element *element = view.root()->GetElementById("control-group-" + name))
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

void wire_document()
{
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
      if (Rml::Element *element = view.root()->GetElementById(binding.id))
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
   wire_arrows(view.root());
   rib_rmlui_wire_lists();
   /* When a design declares screens, we replace these before the first frame,
    * and when it declares none we keep them. In both cases we attach the
    * listeners to the buttons before the player can press anything. */
   rib_rmlui_built_in_screens();

   update_document_state();
   view.show();
}
}

extern "C" bool rib_rmlui_init(
      const char *asset_directory, int width, int height, bool core_context)
{
   if (view.get_context())
      return true;
   if (!view.initialize(asset_directory, width, height, core_context))
      return false;
   wire_document();
   view.settle();
   return true;
}

static Rml::Element *slider_drag = nullptr;

extern "C" void rib_rmlui_shutdown(void)
{
   view.shutdown();
   ActionListener::clear();
   HoverListener::hovered_action = RIB_RMLUI_ACTION_NONE;
   pointer_down = false;
   slider_drag = nullptr;
}

extern "C" void rib_rmlui_capture_next(const char *path)
{
   view.capture_next(path);
}

extern "C" void rib_rmlui_render(int width, int height)
{
   if (!view.get_context())
      return;
   expire_status(main_status, "status");
   expire_status(controls_status, "controls-status");
   view.render(width, height);
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
   if (!state.thumbnail_path.empty())
      view.release_texture(state.thumbnail_path);
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
   if (!view.root() || !markup)
      return false;
   Rml::Element *scene = view.root()->GetElementById("controller-scene");
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
using rib::display_none;
using rib::hidden;
using rib::collect;

static bool rib_under_class_hidden(Rml::Element *element, const char *class_name)
{
   for (Rml::Element *cursor = element; cursor; cursor = cursor->GetParentNode())
      if (cursor->IsClassSet(class_name) && display_none(cursor))
         return true;
   return false;
}

static Rml::Element *rib_visible_list(void)
{
   if (!view.root())
      return nullptr;
   std::vector<Rml::Element*> lists;
   collect(view.root(), "list", lists);
   for (Rml::Element *list : lists)
      if (!hidden(list))
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
   collect(list, "list-row", all);
   for (Rml::Element *row : all)
      if (!rib_under_class_hidden(row, "list-page") && !display_none(row))
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
   if (!view.root())
      return;
   std::vector<Rml::Element*> toggles;
   collect(view.root(), "list-toggle", toggles);
   for (Rml::Element *toggle : toggles)
      toggle->AddEventListener(Rml::EventId::Click, new ToggleListener());
   wire_part_toggles(view.root());
}

extern "C" void rib_rmlui_set_toggle(const char *id, const char *state, bool on)
{
   if (!view.root() || !id || !*id)
      return;
   if (Rml::Element *toggle = view.root()->GetElementById(id))
      toggle->SetClass("on", on);
   if (Rml::Element *word = view.root()->GetElementById(std::string(id) + "-state"))
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
   if (!view.root())
      return;
   std::vector<Rml::Element*> rows;
   collect(view.root(), "list-row", rows);
   for (Rml::Element *row : rows)
      row->AddEventListener(Rml::EventId::Click,
            new ListListener(ListListener::Choose, ""));
   std::vector<Rml::Element*> previous;
   collect(view.root(), "list-pager-prev", previous);
   for (Rml::Element *button : previous)
      button->AddEventListener(Rml::EventId::Click,
            new ListListener(ListListener::Page, "prev"));
   std::vector<Rml::Element*> next;
   collect(view.root(), "list-pager-next", next);
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
   if (!view.root())
      return;
   std::vector<Rml::Element*> all;
   collect(view.root(), "list-row", all);
   for (Rml::Element *row : all)
      row->SetClass("focused", false);
   std::vector<Rml::Element*> rows;
   rib_visible_rows(rows);
   if (index >= 0 && index < (int)rows.size())
      rows[index]->SetClass("focused", true);
}

static Rml::Element *rib_visible_panel(void)
{
   if (!view.root())
      return nullptr;
   std::vector<Rml::Element*> panels;
   collect(view.root(), "screen-panel", panels);
   for (Rml::Element *panel : panels)
      if (!display_none(panel))
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
      collect(panel, name, out);
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
   collect(panel, "list-back", back);
   collect(panel, "options-back", back);
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
   if (!view.root() || screens.empty())
      return id.c_str();
   Rml::Element *pause = view.root()->GetElementById(screens.front().panel);
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
            for (Rml::Element *e = view.root()->GetElementById(one); e;
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
   collect(page, "list-row", rows);
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
      collect(list, arrow.name, found);
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
   collect(list, "list-page", pages);
   std::vector<Rml::Element*> usable;
   for (Rml::Element *page : pages)
      if (rib_page_has_row(page))
         usable.push_back(page);
   if (usable.size() < 2)
      return -1;
   int current = 0;
   for (size_t index = 0; index < usable.size(); ++index)
      if (!display_none(usable[index]))
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
   collect(list, "list-pager-count", counts);
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
   if (!view.root() || !list)
      return;
   std::vector<Rml::Element*> rows;
   collect(list, "list-row", rows);
   for (Rml::Element *row : rows)
   {
      const bool selected = id && row->GetId() == id;
      row->SetClass("selected", selected);
      if (Rml::Element *state = view.root()->GetElementById(row->GetId() + "-state"))
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
   if (view.root() && button && *button)
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
               if (Rml::Element *element = view.root()->GetElementById(one))
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
   if (!view.root() || !id || !*id)
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
      if (Rml::Element *panel = view.root()->GetElementById(screen.panel))
      {
         if (&screen == wanted)
            panel->RemoveProperty("display");
         else
            panel->SetProperty("display", "none");
      }
   if (Rml::Element *heading = view.root()->GetElementById("heading"))
      heading->SetInnerRML(Rml::StringUtilities::EncodeRml(wanted->heading));
   if (!wanted->footer.empty())
      if (Rml::Element *footer = view.root()->GetElementById("footer-hint"))
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
   if (!view.root() || !id)
      return;
   const std::string suffix(id);
   if (Rml::Element *control = view.root()->GetElementById("control-" + suffix))
   {
      control->SetClass("focused", focused);
      control->SetClass("capturing", capturing);
   }
   if (Rml::Element *hit = view.root()->GetElementById("control-hit-" + suffix))
   {
      hit->SetClass("focused", focused);
      hit->SetClass("capturing", capturing);
   }
   if (Rml::Element *label_element =
         view.root()->GetElementById("control-label-" + suffix))
      label_element->SetInnerRML(Rml::StringUtilities::EncodeRml(
            label ? label : ""));
   if (Rml::Element *binding_element =
         view.root()->GetElementById("control-binding-" + suffix))
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
   if (!view.root())
      return;
   if (Rml::Element *element = view.root()->GetElementById("controls-reset"))
      element->SetClass("focused", reset);
   if (Rml::Element *element = view.root()->GetElementById("controls-back"))
      element->SetClass("focused", back);
   if (Rml::Element *element = view.root()->GetElementById("controls-cancel"))
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
   if (!view.root())
      return;
   if (Rml::Element *element = view.root()->GetElementById("footer-hint"))
      element->SetInnerRML(Rml::StringUtilities::EncodeRml(
            hint ? hint : ""));
}

extern "C" void rib_rmlui_set_overlay_mode(bool only_overlays)
{
   if (only_overlays)
      rib_rmlui_clear_intents();
   if (!view.root())
      return;
   if (Rml::Element *body = view.root()->GetElementById("body"))
      body->SetClass("overlay", only_overlays);
}

extern "C" void rib_rmlui_set_overlay(const char *element,
      enum rib_overlay_state state)
{
   if (!view.root() || !element || !*element)
      return;
   if (Rml::Element *overlay = view.root()->GetElementById(element))
   {
      overlay->SetClass("showing", state == RIB_OVERLAY_SHOWING);
      overlay->SetClass("leaving", state == RIB_OVERLAY_LEAVING);
   }
}

/* A slider or a toggle, found by its class in the design. We never describe
 * the markup in the control, and only ask for the part. */
using rib::find_class;

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
   if (view.get_context())
      view.get_context()->Update();
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
   if (view.get_context())
      view.get_context()->Update();
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
   rib::walk(node, [](Rml::Element *element) {
      if (element->IsClassSet("toggle") && !element->GetId().empty())
         element->AddEventListener(Rml::EventId::Click, new PartToggleListener(element->GetId()));
      return rib::Walk::Continue;
   });
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

static void wire_arrows(Rml::Element *node)
{
   rib::walk(node, [](Rml::Element *element) {
      if (element->IsClassSet("volume-arrow"))
      {
         const int direction = element->IsClassSet("arrow-down") ? -1
               : element->IsClassSet("arrow-up") ? 1 : 0;
         Rml::Element *slider = find_class(element->GetParentNode(), "slider", true);
         if (slider && direction != 0)
            element->AddEventListener(Rml::EventId::Click,
                  new ArrowListener(slider->GetId(), direction));
      }
      return rib::Walk::Continue;
   });
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
   if (!view.root() || !id)
      return;
   if (Rml::Element *slider = view.root()->GetElementById(id))
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
   if (!view.root() || !id)
      return false;
   Rml::Element *slider = view.root()->GetElementById(id);
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
   rib::walk(node, [&](Rml::Element *element) {
      if (display_none(element) || element->HasAttribute("disabled")
            || element->IsClassSet("disabled"))
         return rib::Walk::SkipChildren;
      // The pointer arrows are still not keyboard stops.
      const bool part = !element->IsClassSet("volume-arrow")
            && (element->IsClassSet("slider") || element->IsClassSet("toggle")
               || element->IsClassSet("menu-action"));
      if (part && !element->GetId().empty())
         out.push_back(element->GetId());
      return rib::Walk::Continue;
   });
}

extern "C" int rib_rmlui_focusables(const char *panel, char ids[][64], int capacity)
{
   if (!view.root() || !panel || !ids || capacity <= 0)
      return 0;
   Rml::Element *root = view.root()->GetElementById(panel);
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
      if (Rml::Element *element = view.root()->GetElementById(ids[index]))
         element->SetClass("focused", id && std::strcmp(ids[index], id) == 0);
}

extern "C" bool rib_rmlui_part_is_slider(const char *id)
{
   if (!view.root() || !id)
      return false;
   Rml::Element *element = view.root()->GetElementById(id);
   return element && element->IsClassSet("slider");
}

extern "C" void rib_rmlui_pointer_move(int x, int y)
{
   pointer_x = x;
   pointer_y = y;
   if (view.get_context())
      view.get_context()->ProcessMouseMove(x, y, 0);
   if (slider_drag)
      drag_to(x);
}

extern "C" void rib_rmlui_pointer_button(bool down)
{
   if (!view.get_context())
      return;
   if (down == pointer_down)
      return;
   pointer_down = down;
   if (down)
   {
      view.get_context()->ProcessMouseButtonDown(0, 0);
      if (Rml::Element *slider = slider_ancestor(view.get_context()->GetHoverElement()))
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
      view.get_context()->ProcessMouseButtonUp(0, 0);
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
   if (!view.get_context())
      return;
   view.get_context()->ProcessMouseLeave();
   HoverListener::hovered_action = RIB_RMLUI_ACTION_NONE;
   if (slider_drag)
      end_drag();
   if (pointer_down)
   {
      pointer_down = false;
      view.get_context()->ProcessMouseButtonUp(0, 0);
   }
}

extern "C" bool rib_rmlui_element_center(const char *id, int *x, int *y)
{
   if (!view.get_context() || !view.root() || !id || !x || !y)
      return false;
   view.get_context()->Update();
   Rml::Element *element = view.root()->GetElementById(id);
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
   if (!view.get_context() || !view.root() || !id || !x || !y || !w || !h)
      return false;
   view.get_context()->Update();
   Rml::Element *element = view.root()->GetElementById(id);
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
   if (!view.get_context() || !view.root())
      return -1;
   Rml::Element *cursor = view.get_context()->GetHoverElement();
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
   if (!view.root() || !id)
      return false;
   Rml::Element *element = view.root()->GetElementById(id);
   return element && element->HasAttribute("disabled");
}

extern "C" bool rib_rmlui_reload_if_changed(void)
{
   if (!view.reload_if_changed())
      return false;
   wire_document();
   return true;
}

static void rib_set_text(const std::string& id, const char *text)
{
   if (!view.root())
      return;
   if (Rml::Element *element = view.root()->GetElementById(id))
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
   if (!view.root() || !id)
      return;
   if (Rml::Element *element = view.root()->GetElementById(id))
   {
      if (shown)
         element->RemoveProperty("display");
      else
         element->SetProperty("display", "none");
   }
}

extern "C" void rib_rmlui_set_disabled(const char *id, bool disabled)
{
   if (!view.root() || !id)
      return;
   Rml::Element *element = view.root()->GetElementById(id);
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
   Rml::Element *element = view.root() ? view.root()->GetElementById(title_id) : nullptr;
   if (!element || !view.get_context())
   {
      rib_set_text(title_id, source.c_str());
      return;
   }
   view.get_context()->Update();
   const float density = std::max(view.get_context()->GetDensityIndependentPixelRatio(), 0.1f);
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
   if (!view.root() || !list_id)
      return;
   std::vector<Rml::Element*> rows;
   collect(rib_list_element(list_id), "list-row", rows);
   for (Rml::Element *row : rows)
   {
      const bool selected = row_id && row->GetId() == row_id;
      row->SetClass("selected", selected);
      if (Rml::Element *state = view.root()->GetElementById(row->GetId() + "-state"))
         state->SetInnerRML(Rml::StringUtilities::EncodeRml(
               selected ? (on ? on : "") : (off ? off : "")));
   }
}

static Rml::Element *rib_list_element(const char *list_id)
{
   if (!view.root() || !list_id || !*list_id)
      return nullptr;
   return view.root()->GetElementById(list_id);
}

extern "C" int rib_rmlui_rows_in(const char *list_id)
{
   std::vector<Rml::Element*> rows;
   collect(rib_list_element(list_id), "list-row", rows);
   return (int)rows.size();
}

extern "C" const char *rib_rmlui_row_in(const char *list_id, int index)
{
   static std::string id;
   std::vector<Rml::Element*> rows;
   collect(rib_list_element(list_id), "list-row", rows);
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
   collect(list, "list-page", pages);
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
   collect(list, "list-pager", pagers);
   if (pagers.empty())
      return;
   if (usable.size() < 2)
   {
      pagers[0]->SetProperty("display", "none");
      return;
   }
   pagers[0]->RemoveProperty("display");
   std::vector<Rml::Element*> counts;
   collect(list, "list-pager-count", counts);
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
   return rib::specified_dp(element, name, view.get_context());
}

extern "C" void rib_rmlui_place_list(const char *list_id, const char *anchor_id,
      int width_dp)
{
   rib::place_binds_popup(view.root(), view.get_context(), list_id, anchor_id, width_dp);
}

extern "C" int rib_rmlui_controls_covered(const char *anchor_id,
      int left, int top, int width, int height)
{
   return rib::popup_covered_labels(view.root(), view.get_context(), anchor_id, left, top, width, height);
}

extern "C" bool rib_rmlui_pointer_inside(const char *id, int x, int y)
{
   if (!view.root() || !view.get_context() || !id)
      return false;
   Rml::Element *element = view.root()->GetElementById(id);
   if (!element || hidden(element))
      return false;
   view.get_context()->Update();
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
   return view.root() && id && view.root()->GetElementById(id);
}

extern "C" void rib_rmlui_focus_group(const char *group)
{
   if (!view.root())
      return;
   std::vector<Rml::Element*> groups;
   collect(view.root(), "control-group", groups);
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
   if (!view.root() || !view.get_context() || !row_id)
      return false;
   view.get_context()->Update();
   Rml::Element *row = view.root()->GetElementById(row_id);
   if (!row || hidden(row))
      return false;
   auto span = [](Rml::Element *element, bool right_aligned,
         float &left, float &right) -> bool {
      if (!element || display_none(element))
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
   collect(row, "list-row-title", titles);
   collect(row, "list-row-detail", details);
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
   if (!view.root() || !view.get_context() || !id || !x || !y || !w || !h)
      return false;
   view.get_context()->Update();
   Rml::Element *element = view.root()->GetElementById(id);
   if (!element || hidden(element))
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
   collect(view.root(), class_name, found);
   for (Rml::Element *element : found)
      if (!element->GetId().empty() && !hidden(element))
         ++count;
   return count;
}

extern "C" const char *rib_rmlui_test_class_id(const char *class_name, int index)
{
   static std::string id;
   std::vector<Rml::Element*> found;
   int seen = 0;
   id.clear();
   collect(view.root(), class_name, found);
   for (Rml::Element *element : found)
   {
      if (element->GetId().empty() || hidden(element))
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
   view.get_context()->Update();
   auto *element = view.root()->GetElementById(id);
   value = element && element->GetProperty(property) ? element->GetProperty(property)->ToString() : "";
   return value.c_str();
}
#endif

#ifdef RIB_RMLUI_HEADLESS
extern "C" unsigned rib_rmlui_test_texture_loads() { return view.texture_loads(); }
#endif

/* We walk these classes only at explicit script checkpoints, never in normal
 * frames. Observe the visible state, not internal focus indexes or paths. */
extern "C" const char *rib_rmlui_script_report(const char *screen, bool menu_open,
      bool transfer_pending, bool capture_active, const char *profile,
      float volume_db)
{
   static std::string report;
   const auto quote = [](const std::string& value) {
      std::string out = "\"";
      for (unsigned char ch : value)
      {
         if (ch == '\\' || ch == '"') out += '\\';
         if (ch < 32)
         {
            char escaped[7];
            std::snprintf(escaped, sizeof(escaped), "\\u%04x", ch);
            out += escaped;
         }
         else out += ch;
      }
      return out + '"';
   };
   const auto boolean = [](bool value) { return value ? "true" : "false"; };
   report = "{\"screen\":" + quote(screen ? screen : "")
      + ",\"menuOpen\":" + boolean(menu_open)
      + ",\"transferPending\":" + boolean(transfer_pending)
      + ",\"captureActive\":" + boolean(capture_active)
      + ",\"profile\":" + quote(profile ? profile : "")
      + ",\"volumeDb\":" + std::to_string(volume_db);
   for (const char *name : {"focused", "selected", "capturing", "disabled",
         "on", "showing", "leaving"})
   {
      report += "," + quote(name) + ":[";
      std::vector<Rml::Element*> found;
      collect(view.root(), name, found);
      bool comma = false;
      for (Rml::Element *element : found)
      {
         if (element->GetId().empty() || hidden(element)) continue;
         if (comma) report += ',';
         report += quote(element->GetId());
         comma = true;
      }
      report += ']';
   }
   report += ",\"text\":{";
   bool comma = false;
   for (const char *id : {"heading", "footer-hint", "status", "controls-status",
         "controls-device-current", "volume-value", "shaders-page-count",
         "achievements-page-count", "achievement-mode-state", "control-binds"})
   {
      Rml::Element *element = view.root() ? view.root()->GetElementById(id) : nullptr;
      if (!element || hidden(element)) continue;
      if (comma) report += ',';
      report += quote(id) + ':' + quote(element->GetInnerRML());
      comma = true;
   }
   report += "},\"slots\":[";
   for (int index = 0; index < 6; ++index)
   {
      if (index) report += ',';
      report += std::string("{\"occupied\":") + boolean(slots[index].occupied)
         + ",\"thumbnail\":" + boolean(!slots[index].thumbnail_path.empty()) + '}';
   }
   report += "],\"sliders\":{";
   comma = false;
   for (const auto& slider : slider_fraction)
   {
      if (comma) report += ',';
      report += quote(slider.first) + ':' + std::to_string(slider.second);
      comma = true;
   }
   report += "},\"bindsBox\":[";
   Rml::Element *binds = view.root() ? view.root()->GetElementById("control-binds") : nullptr;
   if (binds && !hidden(binds))
   {
      const auto at = binds->GetAbsoluteOffset(Rml::BoxArea::Border);
      const auto size = binds->GetBox().GetSize(Rml::BoxArea::Border);
      for (float value : {at.x, at.y, size.x, size.y})
      {
         if (report.back() != '[') report += ',';
         report += std::to_string(static_cast<int>(std::lround(value)));
      }
   }
   report += "]}";
   return report.c_str();
}

#ifdef RIB_RMLUI_HEADLESS
extern "C" void rib_rmlui_test_advance(double seconds) { view.advance(seconds); }
/* A check for an element that is not there must fail, not crash. If we
 * dereferenced the lookup, one wrong id would end the whole run and hide
 * every check after it. */
extern "C" const char *rib_rmlui_test_text(const char *id) {
   static std::string text;
   Rml::Element *element = view.root() ? view.root()->GetElementById(id) : nullptr;
   text = element ? element->GetInnerRML() : std::string("<no element ") + id + ">";
   return text.c_str();
}
/* Which element has a class, so in a check we test what the player can see
 * and not the internal state of the driver. The focus record in the driver and
 * the classes in the document can differ, and we draw only the classes. */
extern "C" bool rib_rmlui_test_has_class(const char *id, const char *name) {
   Rml::Element *element = view.root() && id ? view.root()->GetElementById(id) : nullptr;
   return element && name && element->IsClassSet(name);
}
extern "C" float rib_rmlui_test_picture_aspect() {
   view.get_context()->Update(); auto size = view.root()->GetElementById("slot-image-1")->GetParentNode()->GetBox().GetSize(Rml::BoxArea::Content); return size.x / size.y;
}
#endif
