#pragma once
#include <functional>

namespace rib {
/* The kind of input the player used last, and the pad of the last press on
 * one. A key or the mouse is the keyboard, and a button, a stick or Home of a
 * pad that plays as player 1 is a pad. Until the first input, a connected pad
 * if there is one, or else the keyboard. */
class Inputs
{
public:
   enum class Kind { Keyboard, Pad };
   /* What we do when the kind or the pad changes. */
   void on_change(std::function<void()> changed) { this->changed = std::move(changed); }
   /* Once a frame, in the menu and during play: a press on a pad, or before
    * the first input, a pad connected. */
   void frame();
   /* A key went down, the mouse moved or a mouse button went down. */
   void keyboard();
   Kind kind() const { return current; }
   /* The joypad index of the pad of the last press on one, or of the
    * connected pad before any input. */
   unsigned pad() const { return last_pad; }
private:
   void use(Kind kind, unsigned pad);
   Kind current = Kind::Keyboard;
   unsigned last_pad = 0;
   bool used = false;
   /* Whether a pad had an input down in the frame before, so that holding
    * one does not take the kind back from a key pressed since. */
   bool pad_down = false;
   std::function<void()> changed;
};
}
