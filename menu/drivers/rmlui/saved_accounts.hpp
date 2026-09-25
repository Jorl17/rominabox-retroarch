#pragma once

#include "declarations.h"
#include "list_owner.hpp"
#include "../../../cheevos/rominabox.h"
#include <string>
#include <vector>

namespace rib {
class Document;
class EventQueue;
class Lists;

/* The QUICK SIGN IN screen, with a row for each account that the player saved
 * in another ROM-in-a-Box game. When the player chooses a row we sign this
 * game in with it, or with FORGET on we remove the account from the list. We
 * state the rows and whether FORGET is on, and the design sets how they look. */
class SavedAccounts : public ListOwner
{
public:
   SavedAccounts(Document& document, Lists& lists, EventQueue& events)
      : document(document), lists(lists), events(events) {}
   /* Find the screen declared with this role. */
   void configure(const rib_design_data& design);
   /* Add the FORGET listener after each document load. */
   void bind();
   ScreenRole role() const override { return ScreenRole::Accounts; }
   void shown() override;
   bool choose(const char *row) override;
   bool act(const char *id) override;
   ScreenRole leave_for() override;
   /* How many accounts are saved now. We also set this as the saved-accounts
    * fact wherever a design shows it. */
   static size_t count(Document& document);
private:
   void fill();
   void set_forgetting(bool on);
   Document& document;
   Lists& lists;
   EventQueue& events;
   std::string screen;
   std::vector<rib_achievements_saved_account_t> accounts;
   bool forgetting = false;
   ScreenRole leaving = ScreenRole::None;
};
}
