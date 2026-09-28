/* The menu's own actions, which the player binds on MENU CONTROLS: what
 * opens and closes the menu, and what confirms and goes back inside it, and
 * how a binding to one is written. Declared once, here.
 *
 * The exporter reads this file (desktop/src-tauri/src/menu_controls.rs): the
 * actions an export's defaults name, what each must keep, and the way a
 * binding is written into menu-controls-defaults.cfg. The menu reads the
 * bindings back by the same names (menu_bindings.cpp), and RetroArch asks the
 * menu which of these a press is (rmlui_bridge.h).
 *
 * RIB_MENU_ACTION(name, "id", keeps)
 *    keeps: what the action never loses, so no one locks themselves out:
 *    Binding, at least one of any kind; Key, at least one keyboard key, so a
 *    keyboard alone always reaches it.
 * RIB_MENU_ACTIONS_SHARE(first, second)
 *    Two actions that do the same inside the menu. An input may be bound to
 *    both, and there it acts once, as `first`. Every other pair holds an
 *    input once: binding it to one takes it from the other.
 * RIB_MENU_BINDING(name, "prefix")
 *    A binding is its kind's prefix and what it names: a key by the name
 *    RetroArch's config gives it (input/input_key_names.inc), or the pad
 *    inputs held together, each a position of the standard pad by
 *    RetroArch's bind name for it (input_player1_<name>), joined by the
 *    chord's separator.
 * RIB_MENU_PAD_CHORD("separator")
 * RIB_MENU_PAD_HOME("id", "bind", "Word")
 *    The one pad input that is no position on the standard pad: each pad's
 *    own menu button, which its RetroArch profile binds to `bind`, and the
 *    word the menu and the builder show for it, whatever a pad calls it. The
 *    positions' words are the builder's (controls.json padPositions), which
 *    the exporter writes beside the bindings.
 * RIB_MENU_CAPTURE_CANCEL("key")
 *    The key that ends a capture unchanged, which a capture never takes. */
/* A reader defines the declarations it reads; the others read as nothing. */
#ifndef RIB_MENU_ACTION
#define RIB_MENU_ACTION(name, id, keeps)
#endif
#ifndef RIB_MENU_ACTIONS_SHARE
#define RIB_MENU_ACTIONS_SHARE(first, second)
#endif
#ifndef RIB_MENU_BINDING
#define RIB_MENU_BINDING(name, prefix)
#endif
#ifndef RIB_MENU_PAD_CHORD
#define RIB_MENU_PAD_CHORD(separator)
#endif
#ifndef RIB_MENU_PAD_HOME
#define RIB_MENU_PAD_HOME(id, bind, word)
#endif
#ifndef RIB_MENU_CAPTURE_CANCEL
#define RIB_MENU_CAPTURE_CANCEL(key)
#endif

/* Opens the menu while the game runs, and inside it goes back, or closes
 * it from the pause screen. */
RIB_MENU_ACTION(Menu, "menu", Key)
/* Presses what has focus. */
RIB_MENU_ACTION(Confirm, "confirm", Binding)
/* Goes back, or closes the menu from the pause screen. */
RIB_MENU_ACTION(Back, "back", Binding)

RIB_MENU_ACTIONS_SHARE(Menu, Back)

RIB_MENU_BINDING(Key, "key:")
RIB_MENU_BINDING(Pad, "pad:")
RIB_MENU_PAD_CHORD("+")
RIB_MENU_PAD_HOME("home", "menu_toggle", "Home")
RIB_MENU_CAPTURE_CANCEL("escape")

#undef RIB_MENU_ACTION
#undef RIB_MENU_ACTIONS_SHARE
#undef RIB_MENU_BINDING
#undef RIB_MENU_PAD_CHORD
#undef RIB_MENU_PAD_HOME
#undef RIB_MENU_CAPTURE_CANCEL
