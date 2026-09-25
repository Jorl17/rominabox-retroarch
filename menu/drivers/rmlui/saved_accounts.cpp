#include "saved_accounts.hpp"
#include "document.hpp"
#include "document_contract.hpp"
#include "events.h"
#include "lists.hpp"
#include "listeners.hpp"
#include "words.hpp"
#include <cstring>

namespace rib {
namespace {
/* The only place where we set the saved-accounts fact. */
size_t state_count(Document& document, size_t saved)
{
   document.show_fact(document_contract::SavedAccountsFact, std::to_string(saved));
   return saved;
}
}

void SavedAccounts::configure(const DesignDeclarations& design)
{
   screen.clear();
   panel.clear();
   for (const ScreenDeclaration& declared : design.screens)
      if (declared.role == role())
      {
         screen = declared.id;
         panel = declared.panel;
         break;
      }
}

void SavedAccounts::bind()
{
   forgetting = false;
   if (auto *forget = document.root() ? document.root()->GetElementById(document_contract::AccountsForget) : nullptr)
      forget->AddEventListener(Rml::EventId::Click,
            new ActionListener(events, Event(RIB_RMLUI_ACTION_LIST_ACTION, document_contract::AccountsForget)));
}

size_t SavedAccounts::count(Document& document)
{
   rib_achievements_saved_account_t found[RIB_ACHIEVEMENTS_SAVED_ACCOUNTS];
   return state_count(document, rib_achievements_saved_accounts(found, RIB_ACHIEVEMENTS_SAVED_ACCOUNTS));
}

void SavedAccounts::fill()
{
   rib_achievements_saved_account_t found[RIB_ACHIEVEMENTS_SAVED_ACCOUNTS];
   const size_t count = rib_achievements_saved_accounts(found, RIB_ACHIEVEMENTS_SAVED_ACCOUNTS);
   std::vector<Lists::Row> rows;
   accounts.assign(found, found + count);
   for (size_t index = 0; index < count; ++index)
   {
      Lists::Row row;
      row.id = document_contract::AccountRowPrefix + std::to_string(index);
      row.title = found[index].display_name;
      row.line = true;
      rows.push_back(row);
   }
   if (!screen.empty())
      lists.replace_rows((screen + document_contract::ListSuffix).c_str(), rows);
   state_count(document, count);
}

void SavedAccounts::set_forgetting(bool on)
{
   forgetting = on;
   document.set_class(document_contract::AccountsForget, document_contract::On, on);
   if (!panel.empty())
      document.set_class(panel.c_str(), document_contract::On, on);
}

void SavedAccounts::shown()
{
   set_forgetting(false);
   leaving = ScreenRole::None;
   fill();
   if (!screen.empty())
      document.set_element_text((screen + document_contract::StatusSuffix).c_str(), "");
}

bool SavedAccounts::choose(const char *row)
{
   const char *prefix = document_contract::AccountRowPrefix;
   if (!row || std::strncmp(row, prefix, std::strlen(prefix)))
      return false;
   const size_t index = (size_t)std::strtoul(row + std::strlen(prefix), nullptr, 10);
   if (index >= accounts.size())
      return false;
   if (forgetting)
   {
      rib_achievements_forget_account(accounts[index].username);
      fill();
      if (accounts.empty())
         leaving = ScreenRole::Achievements;
      return true;
   }
   if (rib_achievements_quick_sign_in(accounts[index].username))
   {
      leaving = ScreenRole::Achievements;
      return true;
   }
   if (!screen.empty())
      document.set_element_text((screen + document_contract::StatusSuffix).c_str(), words::QuickSignInFailed);
   return false;
}

bool SavedAccounts::act(const char *id)
{
   if (!id || std::strcmp(id, document_contract::AccountsForget))
      return false;
   set_forgetting(!forgetting);
   return true;
}

ScreenRole SavedAccounts::leave_for()
{
   const ScreenRole next = leaving;
   leaving = ScreenRole::None;
   return next;
}
}
