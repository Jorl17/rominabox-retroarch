#pragma once
#include "document.hpp"
#include "document_contract.hpp"

#include "events.h"
#include "elements.hpp"
#include <RmlUi/Core.h>
#include <map>
#include <string>
#include <vector>

namespace rib {
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
   /* We read the document from `owner` each time and never store it. With a
    * new video driver, for example after a switch to fullscreen and back, the
    * document is freed, and a stored pointer would remain after it. Without a
    * document nothing is focused. */
   explicit Focus(const Document& owner) : owner(owner) {}

   /* A newly loaded document. We keep nothing from the previous one. */
   void attach()
   {
      painted = nullptr;
      pressed = nullptr;
      held = false;
      region = nullptr;
      outside.clear();
      memory.clear();
   }

   /* The focused leaf, or nothing when the focus in RmlUi is on the document
    * itself, as it is after the document is shown. */
   Rml::Element *current() const
   {
      Rml::Context *context = document() ? document()->GetContext() : nullptr;
      Rml::Element *focused = context ? context->GetFocusElement() : nullptr;
      if (!focused || focused == document() || focused->GetOwnerDocument() != document())
         return nullptr;
      return focused;
   }
   std::string current_id() const
   {
      Rml::Element *focused = current();
      return focused ? focused->GetId() : std::string();
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
      for (; element && element != document(); element = element->GetParentNode())
         if (can_focus(element))
            return element;
      return nullptr;
   }

   bool set(Rml::Element *element)
   {
      if (!stop(element))
         return false;
      /* Focus it again even when the context focus is already on it. In RmlUi
       * navigation starts from the chain of focused children in the document.
       * Hiding the menu breaks that chain while the context focus stays, and
       * Focus() rebuilds the chain without any blur or focus event. */
      if (!element->Focus(true))
         return false;
      paint();
      return true;
   }
   bool set(const char *id)
   {
      return document() && id && *id && set(document()->GetElementById(id));
   }

   /* Every stop in `panel` that can take focus now, in document order. */
   std::vector<Rml::Element*> stops(Rml::Element *panel) const
   {
      std::vector<Rml::Element*> found;
      refresh();
      if (!panel || !can_reach(panel))
         return found;
      walk(panel, [&](Rml::Element *element) {
         if (!element->IsVisible() || element->GetComputedValues().focus() == Rml::Style::Focus::None)
            return Walk::SkipChildren;
         if (stoppable(element))
            found.push_back(element);
         return Walk::Continue;
      });
      return found;
   }

   /* The stop in a panel the design marked `autofocus`, if it marked one. */
   Rml::Element *marked(Rml::Element *panel) const
   {
      for (Rml::Element *element : stops(panel))
         if (element->HasAttribute("autofocus"))
            return element;
      return nullptr;
   }

   /* Where a panel starts: the stop the design marked `autofocus`, else the
    * first stop in document order. */
   Rml::Element *first(Rml::Element *panel) const
   {
      if (Rml::Element *start = marked(panel))
         return start;
      const std::vector<Rml::Element*> found = stops(panel);
      return found.empty() ? nullptr : found.front();
   }

   /* Paint the focused leaf in RmlUi, and only it, as `focused`, and put its
    * name on the document as `data-focus`. */
   void paint()
   {
      Rml::Element *focused = current();
      if (focused != painted.get())
      {
         if (painted)
            painted->SetClass(document_contract::Focused, false);
         if (focused)
            focused->SetClass(document_contract::Focused, true);
         painted = focused ? focused->GetObserverPtr() : Rml::ObserverPtr<Rml::Element>();
      }
      name(focused);
   }
   /* Mark the focused leaf `pressed` while CONFIRM is held: from the press,
    * until the release or until the focus moves on. */
   void press(bool holding)
   {
      Rml::Element *focused = current();
      if (holding && !held && focused)
      {
         focused->SetClass(document_contract::Pressed, true);
         pressed = focused->GetObserverPtr();
      }
      if (pressed && (!holding || pressed.get() != focused))
      {
         pressed->SetClass(document_contract::Pressed, false);
         pressed = nullptr;
      }
      held = holding;
   }
   /* Keep the arrows inside `inside` until a call with nullptr. We mark every
    * element next to it, and next to each of its ancestors, `nav-outside`,
    * which is unfocusable in navigation.rcss. */
   void trap(Rml::Element *inside)
   {
      for (auto& element : outside)
         if (element)
            element->SetClass(document_contract::NavOutside, false);
      outside.clear();
      region = inside ? inside->GetObserverPtr() : Rml::ObserverPtr<Rml::Element>();
      for (Rml::Element *at = inside; at && at != document(); at = at->GetParentNode())
         if (Rml::Element *parent = at->GetParentNode())
            for (int index = 0; index < parent->GetNumChildren(); ++index)
               if (Rml::Element *sibling = parent->GetChild(index); sibling != at)
               {
                  sibling->SetClass(document_contract::NavOutside, true);
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
      if (found == memory.end() || !document())
         return nullptr;
      Rml::Element *element = document()->GetElementById(found->second);
      memory.erase(found);
      return stop(element) ? element : nullptr;
   }
   /* The element to focus when `screen` is shown again, if we still have it. */
   Rml::Element *remembered(const std::string& screen) const
   {
      const auto found = memory.find(screen);
      return found == memory.end() || !document() ? nullptr : document()->GetElementById(found->second);
   }
   void forget(const std::string& screen) { memory.erase(screen); }
   void forget() { memory.clear(); }

private:
   /* We check on every paint, not on a change of leaf, because when a leaf is
    * removed with its screen nothing is focused, and we remove the name too. */
   void name(Rml::Element *focused)
   {
      if (!document())
         return;
      const Rml::String id = focused ? focused->GetId() : Rml::String();
      if (id == document()->GetAttribute<Rml::String>(document_contract::FocusAttribute, ""))
         return;
      if (id.empty())
         document()->RemoveAttribute(document_contract::FocusAttribute);
      else
         document()->SetAttribute(document_contract::FocusAttribute, id);
   }
   /* Bring the styles up to date, because we may not have applied them yet
    * for a panel shown or a class set a moment ago. */
   void refresh() const
   {
      if (Rml::Context *context = document() ? document()->GetContext() : nullptr)
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
      return element && document() && element->GetOwnerDocument() == document()
            && stoppable(element) && can_reach(element);
   }
   const Document& owner;
   Rml::ElementDocument *document() const { return owner.root(); }
   Rml::ObserverPtr<Rml::Element> painted, pressed, region;
   bool held = false;
   std::vector<Rml::ObserverPtr<Rml::Element>> outside;
   std::map<std::string, std::string> memory;
};
}
