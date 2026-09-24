#include "document_contract.hpp"
#include "view.hpp"
#include "listeners.hpp"
namespace rib {
View& menu_view()
{
   static View view;
   return view;
}
bool View::initialize(const char *assets, int width, int height, bool core_context,
      const rib_controls_catalog& controls)
{
   catalog = &controls;
   if (document.get_context()) return true;
   if (!document.initialize(assets, width, height, core_context)) return false;
   wire_document();
   document.settle();
   return true;
}
void View::shutdown()
{
   document.shutdown();
   clear_intents();
   pointer_down = false;
   parts.clear_drag();
   catalog = nullptr;
}
void View::render(int width, int height)
{
   if (!document.get_context()) return;
   status.expire();
   document.render(width, height);
}
bool View::reload_if_changed()
{
   if (!document.reload_if_changed()) return false;
   wire_document();
   return true;
}
void View::wire_toggles()
{
   if (!document.root()) return;
   lists.wire_toggles();
   parts.wire_part_toggles();
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
   /* `opens_screen` means we handle the click through the screen declaration
    * in the design, not through this table. Hover still comes from here,
    * because we track keyboard focus by action, and the player can also reach
    * the two buttons that change screen with the arrow keys. There is nothing
    * to add here for a screen that a design declares later. */
   struct Binding { const char *id; rib_rmlui_action action; bool opens_screen; };
   const Binding bindings[] = {
      {document_contract::Resume, RIB_RMLUI_ACTION_RESUME, false},
      {document_contract::Save, RIB_RMLUI_ACTION_SAVE, false},
      {document_contract::Load, RIB_RMLUI_ACTION_LOAD, false},
      {document_contract::Controls, RIB_RMLUI_ACTION_CONTROLS, true},
      {document_contract::Quit, RIB_RMLUI_ACTION_QUIT, false},
      {document_contract::ControlsBack, RIB_RMLUI_ACTION_CONTROLS_BACK, true},
      {document_contract::ControlsReset, RIB_RMLUI_ACTION_CONTROLS_RESET, false},
      {document_contract::ControlsCancel, RIB_RMLUI_ACTION_CONTROLS_CANCEL, false}
   };

   for (const Binding& binding : bindings)
      if (Rml::Element *element = document.root()->GetElementById(binding.id))
      {
         if (!binding.opens_screen)
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

   controls.wire(*catalog);
   wire_toggles();
   parts.wire_arrows();
   lists.wire_lists();
   /* When a design declares screens, we replace these before the first frame,
    * and when it declares none we keep them. In both cases we attach the
    * listeners to the buttons before the player can press anything. */
   screens.built_in_screens();

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
      document.get_context()->ProcessMouseButtonDown(0, 0);
      parts.begin_drag(document.get_context()->GetHoverElement(), pointer_x);
   }
   else
   {
      parts.drag_to(pointer_x);
      document.get_context()->ProcessMouseButtonUp(0, 0);
      parts.end_drag();
   }
}

void View::pointer_leave()
{
   if (!document.get_context())
      return;
   document.get_context()->ProcessMouseLeave();
   hovered = RIB_RMLUI_ACTION_NONE;
   parts.end_drag();
   if (pointer_down)
   {
      pointer_down = false;
      document.get_context()->ProcessMouseButtonUp(0, 0);
   }
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
