#pragma once
#include "document_contract.hpp"

#include "events.h"
#include "elements.hpp"
#include <RmlUi/Core.h>
#include <map>
#include <string>
#include <vector>

namespace rib {
static inline enum rib_rmlui_action map_menu_toggle(
      bool controls_visible, bool capture_active)
{
   if (capture_active)
      return RIB_RMLUI_ACTION_CONTROLS_CANCEL;
   if (controls_visible)
      return RIB_RMLUI_ACTION_CONTROLS_BACK;
   return RIB_RMLUI_ACTION_RESUME;
}

static inline bool toggle_stays_in_menu(
      bool controls_visible, bool capture_active)
{
   return capture_active || controls_visible;
}

/* A stop on the pad screen, as in Controls: a control by its catalog index,
 * or one of the buttons of the screen. */
struct FocusTarget
{
   enum class Kind { Item, Reset, Back };
   Kind kind = Kind::Item;
   int index = 0;
   static FocusTarget item(int index) { return {Kind::Item, index}; }
   static FocusTarget reset() { return {Kind::Reset, 0}; }
   static FocusTarget back() { return {Kind::Back, 0}; }
   bool operator==(const FocusTarget& other) const
   { return kind == other.kind && index == other.index; }
};

/* We let RmlUi handle the focus. The focusable elements are declared once, in
 * the shared navigation.rcss, which we link before the design's stylesheet,
 * and we use the RmlUi search to move between them. Here we keep what RmlUi
 * does not track: the one element marked `focused` (the RmlUi :focus selector
 * also matches every ancestor), whether an element can be focused now, the
 * first element on each screen, the element to focus on return to a screen,
 * and the area the focus stays in while a dialog or the picker is open. */
class Focus
{
public:
   /* A newly loaded document. We keep nothing from the previous one. */
   void attach(Rml::ElementDocument *loaded)
   {
      document = loaded;
      painted = nullptr;
      seen = nullptr;
      region = nullptr;
      outside.clear();
      memory.clear();
   }

   /* The focused leaf, or nothing when the focus in RmlUi is on the document
    * itself, as it is after the document is shown. */
   Rml::Element *current() const
   {
      Rml::Context *context = document ? document->GetContext() : nullptr;
      Rml::Element *focused = context ? context->GetFocusElement() : nullptr;
      if (!focused || focused == document || focused->GetOwnerDocument() != document)
         return nullptr;
      return focused;
   }
   std::string current_id() const
   {
      Rml::Element *focused = current();
      return focused ? focused->GetId() : std::string();
   }
   /* The id of the focused element, under its name in the pause row. We read
    * it in test_rmlui_interaction.cpp. */
   const std::string& pause_element()
   {
      last_id = current_id();
      return last_id;
   }

   /* Whether the element can be focused now. Element::Focus() ignores
    * tab-index, so we reject a disabled or hidden element here. */
   bool stop(Rml::Element *element) const
   {
      refresh();
      return can_focus(element);
   }
   /* The stop an element belongs to: itself, or the nearest ancestor that is
    * one. A pointer over a stop's label is over the stop. */
   Rml::Element *stop_at(Rml::Element *element) const
   {
      refresh();
      for (; element && element != document; element = element->GetParentNode())
         if (can_focus(element))
            return element;
      return nullptr;
   }

   bool set(Rml::Element *element)
   {
      if (!stop(element))
         return false;
      if (element != current() && !element->Focus(true))
         return false;
      paint();
      return true;
   }
   bool set(const char *id)
   {
      return document && id && *id && set(document->GetElementById(id));
   }

   /* Where a panel starts: the stop the design marked `autofocus`, else the
    * first stop in document order. */
   Rml::Element *first(Rml::Element *panel) const
   {
      Rml::Element *marked = nullptr;
      Rml::Element *earliest = nullptr;
      refresh();
      if (!panel || !can_reach(panel))
         return nullptr;
      walk(panel, [&](Rml::Element *element) {
         if (!element->IsVisible() || element->GetComputedValues().focus() == Rml::Style::Focus::None)
            return Walk::SkipChildren;
         if (!stoppable(element))
            return Walk::Continue;
         if (!earliest)
            earliest = element;
         if (element->HasAttribute("autofocus"))
         {
            marked = element;
            return Walk::Stop;
         }
         return Walk::Continue;
      });
      return marked ? marked : earliest;
   }

   /* Paint RmlUi's focused leaf, and only it, as `focused`. */
   void paint()
   {
      Rml::Element *focused = current();
      if (focused == painted.get())
         return;
      if (painted)
         painted->SetClass(document_contract::Focused, false);
      if (focused)
         focused->SetClass(document_contract::Focused, true);
      painted = focused ? focused->GetObserverPtr() : Rml::ObserverPtr<Rml::Element>();
   }
   /* Whether the focus has moved since the last call. */
   bool moved()
   {
      Rml::Element *focused = current();
      if (focused == seen.get())
         return false;
      seen = focused ? focused->GetObserverPtr() : Rml::ObserverPtr<Rml::Element>();
      return true;
   }

   /* Keep the arrows inside `inside` until a call with nullptr. We mark every
    * element next to it, and next to each of its ancestors, `nav-outside`,
    * which is unfocusable in navigation.rcss. */
   void trap(Rml::Element *inside)
   {
      for (auto& element : outside)
         if (element)
            element->SetClass("nav-outside", false);
      outside.clear();
      region = inside ? inside->GetObserverPtr() : Rml::ObserverPtr<Rml::Element>();
      for (Rml::Element *at = inside; at && at != document; at = at->GetParentNode())
         if (Rml::Element *parent = at->GetParentNode())
            for (int index = 0; index < parent->GetNumChildren(); ++index)
               if (Rml::Element *sibling = parent->GetChild(index); sibling != at)
               {
                  sibling->SetClass("nav-outside", true);
                  outside.push_back(sibling->GetObserverPtr());
               }
   }
   Rml::Element *trapped() const { return region.get(); }

   /* The element a screen returns to when it is shown again. */
   void remember(const std::string& screen)
   {
      memory[screen] = current_id();
   }
   Rml::Element *recall(const std::string& screen)
   {
      const auto found = memory.find(screen);
      if (found == memory.end() || !document)
         return nullptr;
      Rml::Element *element = document->GetElementById(found->second);
      memory.erase(found);
      return stop(element) ? element : nullptr;
   }
   void forget(const std::string& screen) { memory.erase(screen); }
   void forget() { memory.clear(); }

private:
   /* Bring the styles up to date, because we may not have applied them yet
    * for a panel shown or a class set a moment ago. */
   void refresh() const
   {
      if (Rml::Context *context = document ? document->GetContext() : nullptr)
         context->Update();
   }
   static bool stoppable(Rml::Element *element)
   {
      return element->GetComputedValues().tab_index() == Rml::Style::TabIndex::Auto
            && !element->HasAttribute("disabled")
            && !element->IsClassSet(document_contract::Disabled);
   }
   bool can_reach(Rml::Element *element) const
   {
      for (Rml::Element *at = element; at; at = at->GetParentNode())
         if (!at->IsVisible() || at->GetComputedValues().focus() == Rml::Style::Focus::None)
            return false;
      return true;
   }
   bool can_focus(Rml::Element *element) const
   {
      return element && document && element->GetOwnerDocument() == document
            && stoppable(element) && can_reach(element);
   }
   Rml::ElementDocument *document = nullptr;
   Rml::ObserverPtr<Rml::Element> painted, seen, region;
   std::vector<Rml::ObserverPtr<Rml::Element>> outside;
   std::map<std::string, std::string> memory;
   std::string last_id;
};
}
