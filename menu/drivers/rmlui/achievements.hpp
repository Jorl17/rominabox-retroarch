#pragma once
#include "document.hpp"
#include "events.h"
#include "lists.hpp"
#include "overlays.hpp"
#include "text_entry.hpp"
#include "../../../cheevos/rominabox.h"

namespace rib {
/* Presentation only. All account, evaluator, token and retry state is in the
 * native achievements service, and the shared component markup is in designs. */
class Achievements
{
public:
   enum class Exit { Quit, SignOut };
   Achievements(Document& document, Lists& lists, EventQueue& intents, Overlays& overlays)
      : document(document), lists(lists), intents(intents), overlays(overlays), text(document) {}
   void bind();
   void update();
   bool handle(const Event& event);
   bool key(rib_key key);
   bool begin_native_input();
   bool physical(bool down, unsigned key, uint32_t character, uint16_t modifiers);
   void leave_form();
   void context_lost();
   /* The screen has just been shown. The player may have saved an account
    * for QUICK SIGN IN in another game since we last painted it. */
   void shown();
   bool request_exit(Exit exit);
   bool allow_quit();
   bool modal() const { return confirming || snapshot.startup_waiting; }
private:
   void action(AccountAction action);
   void show_form(bool show);
   void paint();
   void paint_rows();
   void sign_in();
   Document& document;
   Lists& lists;
   EventQueue& intents;
   Overlays& overlays;
   TextEntry text;
   rib_achievements_snapshot_t snapshot{};
   uint32_t revision = UINT32_MAX;
   /* The achievement whose popup is shown without its badge, or 0. */
   uint32_t popup_waiting = 0;
   bool form = false, confirming = false, exit_approved = false;
   Exit pending_exit = Exit::Quit;
   int modal_focus = 0;
   std::string account, confirmation_focus;
};
}
