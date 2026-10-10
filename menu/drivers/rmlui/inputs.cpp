#include "inputs.hpp"
#include "host.h"

namespace rib {
void Inputs::frame()
{
   unsigned pad = 0;
   const bool down = rib_host_pad_pressed(&pad);
   if (down && !pad_down)
   {
      used = true;
      use(Kind::Pad, pad);
   }
   else if (!used && rib_host_pad_connected(&pad))
      use(Kind::Pad, pad);
   pad_down = down;
}

void Inputs::keyboard()
{
   used = true;
   use(Kind::Keyboard, last_pad);
}

void Inputs::use(Kind kind, unsigned pad)
{
   if (kind == current && pad == last_pad)
      return;
   current = kind;
   last_pad = pad;
   if (changed)
      changed();
}
}
