#ifndef RIB_MENU_SCRIPT_HPP
#define RIB_MENU_SCRIPT_HPP

#include <cstddef>
#include <cstdint>

namespace rib {
struct ScriptObservation
{
   const char *screen;
   bool transfer_pending;
   bool capture_active;
   const char *profile;
};

/* For test scripts only. We use the element listeners and menu key entry
 * point of normal input, one command per frame, and a settled final capture. */
class Script
{
public:
   void run(void *menu, const ScriptObservation& observation);
   void restore_hover() const;
   bool wants_frames() const { return running; }
   bool has_hover() const { return hover[0] != 0; }
private:
   const char *script = nullptr;
   size_t at = 0;
   bool started = false;
   bool running = false;
   int settle = 8;
   int waiting = 0;
   int64_t wait_until = 0;
   char hover[128]{};
};
}
#endif
