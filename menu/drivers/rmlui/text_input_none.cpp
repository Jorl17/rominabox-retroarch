/* A build with no platform text service: the RmlUi clipboard, and no input
 * method window to place. The headless harness is such a build. */
#include "text_input_platform.hpp"

namespace rib {
std::unique_ptr<TextInputPlatform> make_text_input_platform(Document&)
{
   return nullptr;
}
}
