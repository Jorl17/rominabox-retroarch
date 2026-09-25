#pragma once

namespace rib {
/* The code behind the list on one screen, such as the disc list, the filters
 * or the saved accounts. Here we fill the rows of that screen and handle a
 * chosen row or a button of the screen. In Menu we find the list by the
 * declared role of the screen showing, never by name, and pass it the row.
 * We leave how it looks to the design. */
class ListOwner
{
public:
   virtual ~ListOwner() = default;
   /* The role of the screen of this list, as declared in design.cfg. */
   virtual const char *role() const = 0;
   /* We call this when we have just shown the screen. */
   virtual void shown() {}
   /* The player chose a row. Returns true when we acted, to play the sound. */
   virtual bool choose(const char *row) = 0;
   /* The player pressed one of the buttons of the screen. */
   virtual bool act(const char *id) { (void)id; return false; }
   /* The role of the screen to show after the last choice, or nullptr to
    * stay. We clear it when we return it. */
   virtual const char *leave_for() { return nullptr; }
};
}
