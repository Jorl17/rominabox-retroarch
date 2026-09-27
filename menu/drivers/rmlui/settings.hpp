#ifndef RIB_MENU_SETTINGS_HPP
#define RIB_MENU_SETTINGS_HPP

#include "declarations.h"
#include "events.h"
#include <string>
#include <vector>

namespace rib {
class Document;
class Lists;
class Parts;

/* The settings that the player changes in the Options of the game, as we
 * declare them in the exporter. Each controls one RetroArch key. We apply a
 * change at once through the host, save it in a file in the data folder of
 * the game, and apply that file in the launcher at the next launch. The
 * value is in RetroArch, and here we keep and show the declarations. We
 * disable a setting with no effect in the game, and the design may hide it. */
class PlayerSettings
{
public:
   PlayerSettings(Document& document, Parts& parts, Lists& lists)
      : document(document), parts(parts), lists(lists) {}
   void load(const DesignDeclarations& design, const char *data_directory);
   /* Call once the document is loaded. We set the step of each level, and move
    * a level between two positions onto one. */
   void attach();
   void paint() const;
   /* The player moved a slider or pressed a switch. False for anything else. */
   bool handle(const Event& event);
   /* The player moved the slider `control` to `fraction`. We apply each new
    * position once, with its sound. Pass `persist` when the move is finished
    * (a key, an arrow, a released drag) to save it. False if not a setting. */
   bool slide(const char *control, float fraction, bool persist);
   bool toggle(const char *control);
private:
   const SettingDeclaration *owning(const char *control, SettingKind kind) const;
   float value(const SettingDeclaration& setting) const;
   void set(const SettingDeclaration& setting, float value, bool persist);
   Document& document;
   Parts& parts;
   Lists& lists;
   std::vector<SettingDeclaration> entries;
   std::string data;
};
}
#endif
