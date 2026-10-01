#ifndef RIB_MENU_SCRIPT_HPP
#define RIB_MENU_SCRIPT_HPP

#include <cstddef>
#include <cstdint>
#include <string>

/* We include the test script driver only in test builds, which are every
 * headless program, since those are all tests, and a player built with
 * RIB_MENU_SCRIPT=1. A shipped player has the empty driver below. */
#if defined(RIB_RMLUI_HEADLESS) && !defined(RIB_MENU_SCRIPT)
#define RIB_MENU_SCRIPT
#endif

namespace rib {
class View;
struct ScriptObservation
{
   const char *screen;
   bool transfer_pending;
   bool capture_active;
   const char *profile;
   /* The list of the bindings of a control, as declared in the design. */
   const char *binds_list;
   /* Every overlay declared in the design has appeared and ended. */
   bool overlays_settled;
};

#ifdef RIB_MENU_SCRIPT
/* For test scripts only. We use the element listeners and menu key entry
 * point of normal input, one command per frame, and a settled final capture. */
class Script
{
public:
   explicit Script(View& view) : view(view) {}
   void run(void *menu, const ScriptObservation& observation);
   void restore_hover() const;
   // We report state only at explicit checkpoints, and not in normal frames.
   const char *report(const char *screen, bool menu_open, bool transfer_pending,
         bool capture_active, const char *profile, float volume_db);
   bool wants_frames() const { return running; }
   bool has_hover() const { return !hover.empty(); }
private:
   void shot();
   View& view;
   std::string report_text;
   /* ROMINABOX_MENU_SCRIPT, when it is set. */
   std::string steps;
   bool scripted = false;
   size_t at = 0;
   bool started = false;
   /* For a game that starts at its menu, we open the menu once the game has
    * started. We wait for that, because opening the menu drops the queue. */
   bool awaits_menu = false;
   bool running = false;
   int settle = 8;
   int waiting = 0;
   int64_t wait_until = 0;
   bool waits_for_overlays = false;
   std::string hover;
   std::string binds_list;
};
#else
/* A shipped player has no script to run. */
class Script
{
public:
   explicit Script(View&) {}
   void run(void *, const ScriptObservation&) {}
   void restore_hover() const {}
   bool wants_frames() const { return false; }
};
#endif
}
#endif
