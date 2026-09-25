#include "achievements.hpp"
#include "listeners.hpp"
#include "elements.hpp"
#include "host.h"
#include "sounds.hpp"
#include "../../../input/alt_enter_fullscreen.h"
#include "navigation.hpp"
#include <algorithm>
#include <cstring>
#include <libretro.h>
#include <RmlUi/Core/Elements/ElementFormControlInput.h>

namespace rib {
namespace {
bool busy(rib_achievements_status_t status)
{
   return status == RIB_ACHIEVEMENTS_SIGNING_IN || status == RIB_ACHIEVEMENTS_LOADING;
}
const char *status_text(const rib_achievements_snapshot_t& snapshot)
{
   if (snapshot.upload_failed) return snapshot.error;
   switch (snapshot.status) {
      case RIB_ACHIEVEMENTS_SIGNING_IN: return "Signing in...";
      case RIB_ACHIEVEMENTS_LOADING: return "Loading achievements...";
      case RIB_ACHIEVEMENTS_ACTIVE: return snapshot.pending_upload ? "Earned achievements waiting to sync. Keep the game open." : "";
      case RIB_ACHIEVEMENTS_UNAVAILABLE: case RIB_ACHIEVEMENTS_ERROR: return snapshot.error;
      case RIB_ACHIEVEMENTS_OFF: return "Achievements are off.";
      default: return "";
   }
}
/* The colour badge of achievement `id`, when it is already on disk. */
std::string ready_badge(const rib_achievements_snapshot_t& snapshot, uint32_t id)
{
   rib_achievement_row_t item{};
   for (size_t index = 0; index < snapshot.count; ++index)
      if (rib_achievements_get_row(index, &item) && item.id == id &&
            item.badge == RIB_ACHIEVEMENT_BADGE_READY)
         return item.badge_path;
   return {};
}
Rml::ElementFormControlInput *field(Document& document, const char *id)
{
   return document.root() ? dynamic_cast<Rml::ElementFormControlInput*>(document.root()->GetElementById(id)) : nullptr;
}
/* After the type of an input changes there is a new RmlUi text widget, and
 * its text is placed only when the field is resized, so we would draw it at
 * the corner of the window. We replace the field with a copy of the other
 * type, with the same value, caret and focus. */
void mask_password(Document& document, bool masked)
{
   auto *password = field(document, document_contract::AchievementPassword);
   const Rml::String type = masked ? "password" : "text";
   if (!password || password->GetAttribute<Rml::String>("type", "") == type) return;
   const bool focused = document.get_context()->GetFocusElement() == password;
   int start = 0, end = 0;
   password->GetSelection(&start, &end, nullptr);
   const Rml::String value = password->GetValue();
   Rml::ElementPtr copy = password->Clone();
   copy->SetAttribute("type", type);
   password->GetParentNode()->ReplaceChild(std::move(copy), password);
   if (!(password = field(document, document_contract::AchievementPassword))) return;
   password->SetValue(value);
   document.get_context()->Update();
   if (focused) password->Focus();
   password->SetSelectionRange(start, end);
}
/* Focus the first button of a dialog that has just opened, unless the focus
 * is already in it. We then keep the arrows inside the dialog. */
void focus_dialog(Document& document, const char *id)
{
   auto *dialog = document.root() ? document.root()->GetElementById(id) : nullptr;
   if (!dialog || hidden(dialog)) return;
   for (auto *at = document.get_context()->GetFocusElement(); at; at = at->GetParentNode())
      if (at == dialog) return;
   walk(dialog, [&](Rml::Element *element) {
      if (display_none(element)) return Walk::SkipChildren;
      if (!element->IsClassSet(document_contract::MenuAction) || element->HasAttribute("disabled"))
         return Walk::Continue;
      element->Focus(true);
      return Walk::Stop;
   });
}
}
void Achievements::bind()
{
   if (!document.has_element(document_contract::AchievementsPanel)) return;
   const struct { const char *id; AccountAction action; } bindings[] = {
      {document_contract::AchievementsLogin, AccountAction::Open}, {document_contract::AchievementsSubmit, AccountAction::SignIn},
      {document_contract::AchievementsCancel, AccountAction::Cancel}, {document_contract::AchievementPasswordVisibility, AccountAction::RevealPassword}, {document_contract::AchievementsEnabled, AccountAction::Enable},
      {document_contract::AchievementsSignOut, AccountAction::SignOut}, {document_contract::AchievementsRetry, AccountAction::Retry},
      {document_contract::AchievementsStartupRetry, AccountAction::Retry}, {document_contract::AchievementsSkipStartup, AccountAction::SkipStartup},
      {document_contract::AchievementsKeepSession, AccountAction::KeepSession}, {document_contract::AchievementsEndSession, AccountAction::EndSession}
   };
   for (const auto& binding : bindings)
      if (auto *button = document.root()->GetElementById(binding.id))
         button->AddEventListener(Rml::EventId::Click, new ActionListener(intents, Event::account_action(binding.action)));
   text.bind();
   revision = UINT32_MAX;
   update();
}
void Achievements::show_form(bool show)
{
   form = show;
   paint();
   if (show) {
      text.enable(document_contract::AchievementsForm, document_contract::AchievementsSubmit, document_contract::AchievementsCancel);
      if (auto *username = field(document, document_contract::AchievementUsername)) username->Focus();
   } else {
      text.disable();
      if (auto *password = field(document, document_contract::AchievementPassword)) password->SetValue("");
      mask_password(document, true);
      document.set_element_text(document_contract::AchievementPasswordVisibility, "SHOW");
   }
   paint();
}
void Achievements::leave_form()
{
   if (!form) return;
   if (busy(snapshot.status)) rib_achievements_cancel();
   show_form(false);
}
void Achievements::context_lost()
{
   text.disable();
   form = false;
   revision = UINT32_MAX;
}
void Achievements::paint_rows()
{
   std::vector<Lists::Row> rows;
   for (size_t index = 0; index < snapshot.count; ++index) {
      rib_achievement_row_t item{};
      if (!rib_achievements_get_row(index, &item)) continue;
      const char *state = "LOCKED";
      switch (item.state) {
         case RIB_ACHIEVEMENT_UNLOCKED: state = "EARNED"; break;
         case RIB_ACHIEVEMENT_PENDING_UPLOAD: state = "SYNCING"; break;
         case RIB_ACHIEVEMENT_UNSUPPORTED: state = "UNSUPPORTED"; break;
         default: break;
      }
      Lists::Row::Badge badge = Lists::Row::Badge::None;
      switch (item.badge) {
         case RIB_ACHIEVEMENT_BADGE_LOADING: badge = Lists::Row::Badge::Loading; break;
         case RIB_ACHIEVEMENT_BADGE_READY: badge = Lists::Row::Badge::Ready; break;
         default: break;
      }
      if (item.id == popup_waiting && item.badge == RIB_ACHIEVEMENT_BADGE_READY) {
         overlays.show_badge(item.badge_path);
         popup_waiting = 0;
      }
      rows.push_back({"achievement-" + std::to_string(item.id), item.title, item.description,
            std::to_string(item.points) + " PT / " + state, item.badge_path,
            item.state == RIB_ACHIEVEMENT_UNLOCKED || item.state == RIB_ACHIEVEMENT_PENDING_UPLOAD, badge});
   }
   lists.replace_rows(document_contract::AchievementsList, rows);
}
void Achievements::paint()
{
   const bool signed_out = snapshot.status == RIB_ACHIEVEMENTS_SIGNED_OUT || !snapshot.account[0];
   const bool pending = busy(snapshot.status);
   const bool failed = snapshot.status == RIB_ACHIEVEMENTS_ERROR || snapshot.status == RIB_ACHIEVEMENTS_UNAVAILABLE;
   document.set_shown(document_contract::AchievementsForm, form);
   document.set_shown(document_contract::AchievementsSignedOut, !form && signed_out);
   document.set_shown(document_contract::AchievementsCatalog, !form && !signed_out);
   document.set_shown(document_contract::AchievementsSessionActions, !form && !signed_out);
   document.set_shown(document_contract::AchievementsBack, !form);
   document.set_element_text(document_contract::AchievementsAccount, snapshot.account[0] ? snapshot.account : "RETROACHIEVEMENTS");
   document.set_element_text(document_contract::AchievementsState, snapshot.status == RIB_ACHIEVEMENTS_ACTIVE ? "ON" : pending ? "CONNECTING" : "OFF");
   document.set_element_text(document_contract::AchievementsEnabled, snapshot.status == RIB_ACHIEVEMENTS_OFF ? "TURN ON" : pending ? "CANCEL" : "TURN OFF");
   document.set_shown(document_contract::AchievementsEnabled, !failed);
   document.set_shown(document_contract::AchievementsRetry, failed);
   document.set_disabled(document_contract::AchievementsSubmit, pending);
   document.set_disabled(document_contract::AchievementUsername, pending);
   document.set_disabled(document_contract::AchievementPassword, pending);
   document.set_element_text(document_contract::AchievementsStatus, form ? (pending ? status_text(snapshot) : "") : status_text(snapshot));
   document.set_element_text(document_contract::AchievementsError, form && !pending ? snapshot.error : "");
   if (form) {
      if (modal()) text.disable();
      else text.enable(document_contract::AchievementsForm, document_contract::AchievementsSubmit, document_contract::AchievementsCancel);
   }
   document.set_shown(document_contract::AchievementsConfirmation, confirming);
   document.set_element_text(document_contract::AchievementsEndSession, pending_exit == Exit::Quit ? "QUIT ANYWAY" : "SIGN OUT ANYWAY");
   document.set_shown(document_contract::AchievementsStartup, snapshot.startup_waiting);
   document.set_shown(document_contract::AchievementsStartupRetry, snapshot.startup_waiting && failed);
   document.set_element_text(document_contract::AchievementsStartupStatus, failed ? snapshot.error :
         (snapshot.startup_skipped || snapshot.status == RIB_ACHIEVEMENTS_ACTIVE) ? "Restoring your game..." : "Connecting to RetroAchievements...");
}
void Achievements::update()
{
   if (!document.has_element(document_contract::AchievementsPanel)) return;
   // First, so that when we retry a badge because the list is shown again,
   // we request it for the rows in this frame.
   auto *list = document.root()->GetElementById(document_contract::AchievementsList);
   rib_achievements_list_shown(list && !hidden(list));
   rib_achievements_get_snapshot(&snapshot);
   if (snapshot.revision != revision) {
      revision = snapshot.revision;
      if (account != snapshot.account) { overlays.clear_notification(); account = snapshot.account; }
      if (form && (snapshot.status == RIB_ACHIEVEMENTS_ACTIVE ||
            snapshot.status == RIB_ACHIEVEMENTS_UNAVAILABLE ||
            (snapshot.status == RIB_ACHIEVEMENTS_ERROR && snapshot.account[0]))) show_form(false);
      paint();
      paint_rows();
      if (snapshot.startup_waiting) focus_dialog(document, document_contract::AchievementsStartup);
   }
   text.update();
   if (!overlays.notification_active()) {
      rib_achievement_unlock_t unlocked{};
      popup_waiting = 0;
      if (rib_achievements_take_unlock(&unlocked)) {
         // For a popup queued behind another, the badge may already be on disk.
         const std::string badge = unlocked.badge_path[0] ? std::string(unlocked.badge_path)
               : ready_badge(snapshot, unlocked.id);
         overlays.notify({unlocked.title, std::to_string(unlocked.points) + " points", badge});
         // Earned just now, so we are still downloading the colour badge.
         if (badge.empty()) popup_waiting = unlocked.id;
      }
   }
}
void Achievements::sign_in()
{
   auto *username = field(document, document_contract::AchievementUsername);
   auto *password = field(document, document_contract::AchievementPassword);
   if (!username || !password || busy(snapshot.status)) return;
   std::string value = password->GetValue();
   if (username->GetValue().empty() || value.empty()) {
      document.set_element_text(document_contract::AchievementsError, "Enter your username and password."); return;
   }
   const bool started = rib_achievements_sign_in(username->GetValue().c_str(), value.c_str());
   std::fill(value.begin(), value.end(), '\0');
   password->SetValue("");
   if (!started) document.set_element_text(document_contract::AchievementsError, "Could not start sign in. Check your details and try again.");
   update();
}
bool Achievements::request_exit(Exit exit)
{
   if (!rib_achievements_has_pending_uploads()) return false;
   if (!confirming) {
      auto *focused = document.get_context()->GetFocusElement();
      confirmation_focus = form && focused ? focused->GetId() : "";
   }
   pending_exit = exit; confirming = true;
   paint();
   focus_dialog(document, document_contract::AchievementsConfirmation);
   return true;
}
bool Achievements::allow_quit()
{
   if (exit_approved) { exit_approved = false; return true; }
   return !request_exit(Exit::Quit);
}
void Achievements::action(AccountAction wanted)
{
   switch (wanted) {
      case AccountAction::Open: show_form(true); break;
      case AccountAction::SignIn: sign_in(); break;
      case AccountAction::Cancel: rib_achievements_cancel(); show_form(false); break;
      case AccountAction::RevealPassword:
         if (auto *password = field(document, document_contract::AchievementPassword)) {
            const bool reveal = password->GetAttribute<std::string>("type", "password") == "password";
            mask_password(document, !reveal);
            document.set_element_text(document_contract::AchievementPasswordVisibility, reveal ? "HIDE" : "SHOW");
         }
         break;
      case AccountAction::Enable:
         rib_achievements_set_enabled(snapshot.status == RIB_ACHIEVEMENTS_OFF); break;
      case AccountAction::SignOut:
         if (!request_exit(Exit::SignOut)) rib_achievements_sign_out(); break;
      case AccountAction::Retry: rib_achievements_retry(); break;
      case AccountAction::SkipStartup: rib_achievements_skip_startup(); break;
      case AccountAction::KeepSession:
         confirming = false; paint();
         if (form && !confirmation_focus.empty())
            if (auto *focused = document.root()->GetElementById(confirmation_focus)) focused->Focus();
         confirmation_focus.clear();
         break;
      case AccountAction::EndSession:
         confirming = false;
         if (pending_exit == Exit::Quit) { exit_approved = true; rib_host_quit(); }
         else rib_achievements_sign_out();
         paint(); break;
   }
   update();
}
bool Achievements::handle(const Event& event)
{
   if (modal()) {
      if (event.kind != RIB_RMLUI_ACTION_ACCOUNT) return true;
      const bool permitted = confirming ?
            (event.account == AccountAction::KeepSession || event.account == AccountAction::EndSession) :
            (event.account == AccountAction::SkipStartup || event.account == AccountAction::Retry);
      if (!permitted) return true;
   }
   if (event.kind != RIB_RMLUI_ACTION_ACCOUNT) return false;
   action(event.account); return true;
}
bool Achievements::key(rib_key key)
{
   if (modal()) {
      /* We keep the focus inside an open dialog, so the arrows and OK work as
       * usual there. With Back the player keeps playing, except while we are
       * still restoring the game, when we ignore it. */
      if (key == RIB_KEY_CANCEL || key == RIB_KEY_TOGGLE || key == RIB_KEY_RESUME) {
         if (confirming) action(AccountAction::KeepSession);
         return true;
      }
      if (key == RIB_KEY_OK || key == RIB_KEY_SELECT) {
         focus_dialog(document, confirming ? document_contract::AchievementsConfirmation : document_contract::AchievementsStartup);
         if (auto *focused = document.get_context()->GetFocusElement()) focused->Click();
         return true;
      }
      return key == RIB_KEY_START;
   }
   if (form && (key == RIB_KEY_CANCEL || key == RIB_KEY_TOGGLE || key == RIB_KEY_RESUME)) {
      if (text.keyboard_open()) text.cancel_keyboard(); else action(AccountAction::Cancel);
      return true;
   }
   return form && text.controller(key);
}
bool Achievements::physical(bool down, unsigned key, uint32_t character, uint16_t modifiers)
{
   if (!modal()) return text.physical(down, key, character, modifiers);
   if (alt_enter_is_chord(key, modifiers)) return false;
   if (!down) return true;
   switch (key) {
      case RETROK_RETURN: case RETROK_KP_ENTER: this->key(RIB_KEY_OK); break;
      case RETROK_ESCAPE: this->key(RIB_KEY_CANCEL); break;
      case RETROK_LEFT: navigate(document.get_context(), RIB_KEY_LEFT); break;
      case RETROK_UP: navigate(document.get_context(), RIB_KEY_UP); break;
      case RETROK_RIGHT: navigate(document.get_context(), RIB_KEY_RIGHT); break;
      case RETROK_DOWN: navigate(document.get_context(), RIB_KEY_DOWN); break;
      default: break;
   }
   return true;
}
}

namespace rib {
bool Achievements::begin_native_input()
{
   return form && !modal() && text.begin_native_input();
}
}
