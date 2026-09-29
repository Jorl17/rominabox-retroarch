#include "document_contract.hpp"
#include "view.hpp"
#include "sounds.hpp"
#include "listeners.hpp"
namespace rib {
View& menu_view()
{
   static View view;
   return view;
}
bool View::initialize(const char *assets, const std::vector<std::string>& fonts,
      int width, int height, bool core_context, const rib_controls_catalog& controls)
{
   catalog = &controls;
   if (document.get_context()) return true;
   if (!document.initialize(assets, fonts, width, height, core_context)) return false;
   wire_document();
   document.settle();
   return true;
}

void View::shutdown()
{
   document.shutdown();
   clear_intents();
   pointer_down = false;
   parts.forget();
   catalog = nullptr;
}
void View::render(int width, int height)
{
   if (!document.get_context()) return;
   status.expire();
   document.render(width, height);
}
void View::clear_intents()
{
   intents.clear();
   hovered = RIB_RMLUI_ACTION_NONE;
}
void View::set_overlay_mode(bool only_overlays)
{
   if (only_overlays) clear_intents();
   document.set_class(document_contract::Body, document_contract::Overlay, only_overlays);
}
void View::wire_document()
{
   focus.attach();
   /* `opens_screen` means we handle the click through the screen declaration
    * in the design, not through this table. Hover still comes from here,
    * because during capture we read which button is under the pointer. There
    * is nothing to add here for a screen that a design declares later. */
   struct Binding { const char *id; rib_rmlui_action action; bool opens_screen; };
   const Binding bindings[] = {
      {document_contract::Resume, RIB_RMLUI_ACTION_RESUME, false},
      {document_contract::Save, RIB_RMLUI_ACTION_SAVE, false},
      {document_contract::Load, RIB_RMLUI_ACTION_LOAD, false},
      {document_contract::Quit, RIB_RMLUI_ACTION_QUIT, false},
      {document_contract::ForgetConfirm, RIB_RMLUI_ACTION_FORGET, false},
      {document_contract::ControlsBack, RIB_RMLUI_ACTION_CONTROLS_BACK, true},
      {document_contract::ForgetBack, RIB_RMLUI_ACTION_CONTROLS_BACK, true},
      {document_contract::ControlsReset, RIB_RMLUI_ACTION_CONTROLS_RESET, false},
      {document_contract::ControlsCancel, RIB_RMLUI_ACTION_CONTROLS_CANCEL, false}
   };

   for (const Binding& binding : bindings)
      if (Rml::Element *element = document.root()->GetElementById(binding.id))
      {
         if (binding.action == RIB_RMLUI_ACTION_CONTROLS_BACK)
            element->AddEventListener(Rml::EventId::Click, new ReturnListener(intents));
         else if (!binding.opens_screen)
            element->AddEventListener(Rml::EventId::Click,
                  new ActionListener(intents, binding.action));
         element->AddEventListener(Rml::EventId::Mouseover,
               new HoverListener(hovered, binding.action));
         element->AddEventListener(Rml::EventId::Mouseout,
               new HoverListener(hovered, binding.action));
      }

   for (int slot = 1; slot <= rib::kSlotCount; ++slot)
      if (Rml::Element *element = document.root()->GetElementById(document_contract::Slot + std::to_string(slot)))
      {
         const auto event = rib::Event::select_slot(slot);
         element->AddEventListener(Rml::EventId::Click, new ActionListener(intents, event));
         element->AddEventListener(Rml::EventId::Mouseover, new HoverListener(hovered, event));
         element->AddEventListener(Rml::EventId::Mouseout, new HoverListener(hovered, event));
      }

   lists.paginate_all();
   controls.wire(*catalog);
   parts.wire_part_toggles();
   parts.wire_arrows();
   lists.wire_lists();

   slots.paint();
   document.show();
}

void View::pointer_move(int x, int y)
{
   pointer_x = x;
   pointer_y = y;
   if (document.get_context())
      document.get_context()->ProcessMouseMove(x, y, 0);
   parts.drag_to(x);
}

void View::pointer_button(bool down)
{
   if (!document.get_context())
      return;
   if (down == pointer_down)
      return;
   pointer_down = down;
   if (down)
   {
      /* In RmlUi a press focuses the stop under it. */
      document.get_context()->ProcessMouseButtonDown(0, 0);
      focus.paint();
      parts.begin_drag(document.get_context()->GetHoverElement(), pointer_x);
   }
   else
   {
      parts.drag_to(pointer_x);
      document.get_context()->ProcessMouseButtonUp(0, 0);
      intents.push(parts.end_drag());
   }
}

Event View::pointer_leave()
{
   if (!document.get_context())
      return {};
   document.get_context()->ProcessMouseLeave();
   hovered = RIB_RMLUI_ACTION_NONE;
   pointer_settled = false;
   Event cut_short = parts.end_drag();
   if (pointer_down)
   {
      pointer_down = false;
      document.get_context()->ProcessMouseButtonUp(0, 0);
   }
   return cut_short;
}

void View::follow_pointer()
{
   if (!document.get_context())
      return;
   const bool moved = pointer_settled && (pointer_x != settled_x || pointer_y != settled_y);
   pointer_settled = true;
   settled_x = pointer_x;
   settled_y = pointer_y;
   if (!moved)
      return;
   /* When the pointer moves onto another stop, we move the focus and play
    * one cue, the same wherever the stop is. While the player types in a
    * field, the keyboard focus stays there. Moving the pointer over a field
    * does not move the keyboard focus to it, but pressing on it does. */
   Rml::Element *before = focus.current();
   Rml::Element *to = focus.stop_at(document.get_context()->GetHoverElement());
   if (edits_text(before) || edits_text(to))
      return;
   if (focus.set(to) && to != before)
      play_move_sound(false);
}

bool View::move_pointer_to(const char *id)
{
   int x = 0;
   int y = 0;
   if (!document.element_center(id, &x, &y))
      return false;
   pointer_move(x, y);
   return true;
}
}
