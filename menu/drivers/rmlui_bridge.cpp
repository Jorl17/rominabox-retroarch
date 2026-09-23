#include "rmlui_bridge.h"
#include "rmlui/elements.hpp"
#include "rmlui/document.hpp"
#include "rmlui/binds_popup.hpp"
#include "rmlui/slots.hpp"
#include "rmlui/lists.hpp"
#include "rmlui/parts.hpp"
#include "rmlui/focus.hpp"
#include "rmlui/screens.hpp"
#include "rmlui/status.hpp"

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

namespace
{
rib::Document view;
rib::Focus *focus = nullptr;
const rib_controls_catalog *control_declarations = nullptr;
rib::EventQueue intents;
rib::Lists lists(view, intents);
rib::Parts parts(view, intents);
rib::Status status_view(view);
std::unique_ptr<rib::Slots> slot_view;

class ActionListener : public Rml::EventListener
{
public:
   explicit ActionListener(rib::Event action) : action(std::move(action)) {}

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

   static void queue_action(rib::Event event) { intents.push(std::move(event)); }
   static rib::Event take_event() { return intents.take(); }
   static void clear() { intents.clear(); }

private:
   rib::Event action;
};

class HoverListener : public Rml::EventListener
{
public:
   explicit HoverListener(rib::Event action) : action(std::move(action)) {}

   void ProcessEvent(Rml::Event& event) override
   {
      if (event.GetId() == Rml::EventId::Mouseout)
      {
         if (hovered_action.same_target(action))
            hovered_action = RIB_RMLUI_ACTION_NONE;
         return;
      }
      hovered_action = action;
   }
   void OnDetach(Rml::Element*) override { delete this; }

   static rib::Event hovered_action;

private:
   rib::Event action;
};

rib::Event HoverListener::hovered_action = RIB_RMLUI_ACTION_NONE;
rib::Screens screen_view(view, intents, HoverListener::hovered_action);

class DeviceOptionListener : public Rml::EventListener
{
public:
   explicit DeviceOptionListener(std::string id) : id(std::move(id)) {}

   void ProcessEvent(Rml::Event&) override
   {
      ActionListener::queue_action({RIB_RMLUI_ACTION_DEVICE_PICKER_CHOOSE, id});
   }
   void OnDetach(Rml::Element*) override { delete this; }

private:
   std::string id;
};

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

bool pointer_down = false;

void update_document_state() { slot_view->paint(); }

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

   for (int index = 0; index < control_declarations->device_count; ++index)
   {
      const char *id = control_declarations->devices[index].id;
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
   for (int index = 0; index < control_declarations->device_count; ++index)
   {
      const char *id = control_declarations->devices[index].id;
      if (!id || !*id)
         continue;
      if (Rml::Element *option =
            view.root()->GetElementById("controls-device-option-" + std::string(id)))
         option->SetClass("selected", chosen && !std::strcmp(chosen, id));
   }
   if (Rml::Element *current = view.root()->GetElementById("controls-device-current"))
      for (int index = 0; index < control_declarations->device_count; ++index)
         if (chosen && !std::strcmp(chosen, control_declarations->devices[index].id))
         {
            const char *name = control_declarations->devices[index].name;
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
   for (int index = 0; index < control_declarations->count; ++index)
   {
      const char *control_id = control_declarations->entries[index].id;
      if (!control_id || !*control_id)
         break;
      const rib::Event action{RIB_RMLUI_ACTION_CONTROL, control_id};
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
      const char *group = control_declarations->entries[index].group;
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
   screen_view.built_in_screens();
}

void wire_document()
{
   /* `opens_screen` means we handle the click through the screen declaration
    * in the design, not through this table. Hover still comes from here,
    * because we track keyboard focus by action, and the player can also reach
    * the two buttons that change screen with the arrow keys. There is nothing
    * to add here for a screen that a design declares later. */
   struct Binding { const char *id; rib_rmlui_action action; bool opens_screen; };
   const Binding bindings[] = {
      {"resume", RIB_RMLUI_ACTION_RESUME, false},
      {"save", RIB_RMLUI_ACTION_SAVE, false},
      {"load", RIB_RMLUI_ACTION_LOAD, false},
      {"controls", RIB_RMLUI_ACTION_CONTROLS, true},
      {"quit", RIB_RMLUI_ACTION_QUIT, false},
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

   for (int slot = 1; slot <= rib::kSlotCount; ++slot)
      if (Rml::Element *element = view.root()->GetElementById("slot-" + std::to_string(slot)))
      {
         const auto event = rib::Event::select_slot(slot);
         element->AddEventListener(Rml::EventId::Click, new ActionListener(event));
         element->AddEventListener(Rml::EventId::Mouseover, new HoverListener(event));
         element->AddEventListener(Rml::EventId::Mouseout, new HoverListener(event));
      }

   rib_rmlui_wire_controls();
   rib_rmlui_wire_toggles();
   parts.wire_arrows();
   rib_rmlui_wire_lists();
   /* When a design declares screens, we replace these before the first frame,
    * and when it declares none we keep them. In both cases we attach the
    * listeners to the buttons before the player can press anything. */
   rib_rmlui_built_in_screens();

   update_document_state();
   view.show();
}
}

rib::Screens& rib_rmlui_screens() { return screen_view; }

rib::Slots& rib_rmlui_bind_state(rib::Focus& state, const rib_controls_catalog& controls)
{
   focus = &state;
   control_declarations = &controls;
   if (!slot_view) slot_view = std::make_unique<rib::Slots>(view, state, status_view);
   else slot_view->bind_focus(state);
   return *slot_view;
}

bool rib_rmlui_init(
      const char *asset_directory, int width, int height, bool core_context, rib::Focus& state, const rib_controls_catalog& controls)
{
   rib_rmlui_bind_state(state, controls);
   if (view.get_context())
      return true;
   if (!view.initialize(asset_directory, width, height, core_context))
      return false;
   wire_document();
   view.settle();
   return true;
}


extern "C" void rib_rmlui_shutdown(void)
{
   view.shutdown();
   ActionListener::clear();
   HoverListener::hovered_action = RIB_RMLUI_ACTION_NONE;
   pointer_down = false;
   parts.clear_drag();
}

extern "C" void rib_rmlui_capture_next(const char *path)
{
   view.capture_next(path);
}

extern "C" void rib_rmlui_render(int width, int height)
{
   if (!view.get_context())
      return;
   status_view.expire();
   view.render(width, height);
}

extern "C" void rib_rmlui_set_selected_slot(int slot)
{
   return slot_view->set_selected_slot(slot);
}

void rib_rmlui_set_focused(const rib::Event& focused)
{
   focus->pause_action(focused);
   update_document_state();
}

/* Focus one button of the pause row, by its id in the document. */
extern "C" void rib_rmlui_focus_element(const char *id)
{
   focus->pause_element(id);
   update_document_state();
}

extern "C" const char *rib_rmlui_focused_element(void)
{
   return focus->pause_element().c_str();
}

extern "C" void rib_rmlui_set_slot_state(int slot, bool occupied,
      const char *thumbnail_path)
{
   return slot_view->set_slot_state(slot, occupied, thumbnail_path);
}

extern "C" void rib_rmlui_set_game_aspect(float aspect)
{
   return slot_view->set_game_aspect(aspect);
}

extern "C" void rib_rmlui_set_status(const char *status)
{
   status_view.set_main(status);
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

/* A generated list is one class of row and one class of page. The list is on
 * whichever panel is shown, and in the bridge we do not know whether the rows
 * are shaders, achievements or anything else. */
using rib::display_none;
using rib::hidden;
using rib::collect;




extern "C" void rib_rmlui_wire_toggles(void)
{
   if (!view.root()) return;
   lists.wire_toggles();
   parts.wire_part_toggles();
}

extern "C" void rib_rmlui_set_toggle(const char *id, const char *state, bool on)
{
   return lists.set_toggle(id, state, on);
}

extern "C" void rib_rmlui_guard_slots(const char *label, const char *reason)
{
   return slot_view->guard_slots(label, reason);
}

extern "C" bool rib_rmlui_slots_guarded(void)
{
   return slot_view->slots_guarded();
}

extern "C" void rib_rmlui_wire_lists(void)
{
   return lists.wire_lists();
}

extern "C" int rib_rmlui_visible_row_count(void)
{
   return lists.visible_row_count();
}

extern "C" void rib_rmlui_focus_list_row(int index)
{
   return lists.focus_list_row(index);
}

extern "C" bool rib_rmlui_click_screen_back(void)
{
   return lists.click_screen_back();
}

/* The button on the pause row that opens a screen.
 *
 * The fourth button of the pause row is for the screen the design puts there,
 * and with Options in the game it is not the controls button. So to focus and
 * press it, we find the element and do not use the name of a screen.
 */
extern "C" const char *rib_rmlui_pause_screen_button(void)
{
   return screen_view.pause_screen_button();
}

extern "C" int rib_rmlui_list_control_count(void)
{
   return lists.list_control_count();
}

extern "C" const char * rib_rmlui_list_control_id(int index)
{
   return lists.list_control_id(index);
}

extern "C" void rib_rmlui_focus_list_control(int index)
{
   return lists.focus_list_control(index);
}

extern "C" const char * rib_rmlui_list_row_id(int index)
{
   return lists.list_row_id(index);
}

extern "C" int rib_rmlui_turn_list_page(int delta)
{
   return lists.turn_list_page(delta);
}

extern "C" void rib_rmlui_mark_row(const char *id, const char *on, const char *off)
{
   return lists.mark_row(id, on, off);
}

extern "C" void rib_rmlui_clear_screens(void)
{
   return screen_view.clear_screens();
}

extern "C" void rib_rmlui_declare_screen(const char *id, const char *panel,
      const char *heading, const char *footer, const char *button)
{
   return screen_view.declare_screen(id, panel, heading, footer, button);
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
   return screen_view.show_screen(id);
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
   status_view.set_controls(status);
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
   return screen_view.set_footer_hint(hint);
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

static int pointer_x = 0;
static int pointer_y = 0;

extern "C" const char *rib_rmlui_screen_panel(const char *id)
{
   return screen_view.screen_panel(id);
}

extern "C" void rib_rmlui_set_slider(const char *id, float fraction, const char *readout)
{
   return parts.set_slider(id, fraction, readout);
}

extern "C" void rib_rmlui_set_slider_step(const char *id, float step)
{
   return parts.set_slider_step(id, step);
}

extern "C" bool rib_rmlui_nudge_slider(const char *id, int direction)
{
   return parts.nudge_slider(id, direction);
}

extern "C" bool rib_rmlui_commit_slider(const char *id, float fraction)
{
   return parts.commit_slider(id, fraction);
}

extern "C" bool rib_rmlui_slider_drag(const char **id, float *fraction)
{
   return parts.slider_drag(id, fraction);
}

extern "C" int rib_rmlui_focusables(const char *panel, char ids[][64], int capacity)
{
   return rib::focusable_ids(view.root(), panel, ids, capacity);
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
   return parts.part_is_slider(id);
}

extern "C" void rib_rmlui_pointer_move(int x, int y)
{
   pointer_x = x;
   pointer_y = y;
   if (view.get_context())
      view.get_context()->ProcessMouseMove(x, y, 0);
   parts.drag_to(x);
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
      parts.begin_drag(view.get_context()->GetHoverElement(), pointer_x);
   }
   else
   {
      parts.drag_to(pointer_x);
      view.get_context()->ProcessMouseButtonUp(0, 0);
      parts.end_drag();
   }
}

rib::Event rib_rmlui_take_event()
{
   return ActionListener::take_event();
}

rib::Event rib_rmlui_hovered_event()
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
   parts.end_drag();
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
   return lists.hovered_list_row();
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

extern "C" void rib_rmlui_set_row_text(const char *id, const char *title, const char *detail, const char *state)
{
   return lists.set_row_text(id, title, detail, state);
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

extern "C" void rib_rmlui_fit_row_title(const char *id, const char *text)
{
   return lists.fit_row_title(id, text);
}

extern "C" void rib_rmlui_select_row(const char *list_id, const char *row_id, const char *on, const char *off)
{
   return lists.select_row(list_id, row_id, on, off);
}

extern "C" int rib_rmlui_rows_in(const char *list_id)
{
   return lists.rows_in(list_id);
}

extern "C" const char * rib_rmlui_row_in(const char *list_id, int index)
{
   return lists.row_in(list_id, index);
}

extern "C" void rib_rmlui_retarget_pages(const char *list_id)
{
   return lists.retarget_pages(list_id);
}

extern "C" void rib_rmlui_place_list(const char *list_id, const char *anchor_id, int width_dp)
{
   return lists.place_list(list_id, anchor_id, width_dp);
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
   for (int index = 0; index < rib::kSlotCount; ++index)
   {
      if (index) report += ',';
      report += std::string("{\"occupied\":") + boolean(slot_view->occupied(index + 1))
         + ",\"thumbnail\":" + boolean(slot_view->has_thumbnail(index + 1)) + '}';
   }
   report += "],\"sliders\":{";
   comma = false;
   for (const auto& slider : parts.fractions())
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
