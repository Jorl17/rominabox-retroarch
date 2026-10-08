/* The hotkeys, which the player binds on HOTKEYS: what opens and closes the
 * menu, what confirms and goes back inside it, and what saves, loads and
 * changes the save slot while the game plays; and how a binding to one is
 * written. Declared once, here.
 *
 * The exporter reads this file (desktop/src-tauri/src/hotkeys.rs): the
 * hotkeys an export's defaults name, what each must keep, when each acts,
 * and the way a binding is written into hotkeys-defaults.cfg. The builder
 * reads it too (desktop/src/hotkeys.ts). The menu reads the bindings back by
 * the same names (hotkey_bindings.cpp), and RetroArch asks the menu which of
 * these a press is (rmlui_bridge.h).
 *
 * RIB_HOTKEY(name, "id", keeps, acts)
 *    keeps: what the hotkey never loses, so no one locks themselves out:
 *    Binding, at least one of any kind; Key, at least one keyboard key, so a
 *    keyboard alone always reaches it; Nothing, it may be left unbound.
 *    acts: when it does what it does: InMenu, while the menu is open;
 *    InGame, while the game plays; Both. A hotkey that acts while the game
 *    plays holds no key the game itself reads and no pad button it reads
 *    alone, or a press would do both. A chord of several buttons, and
 *    Home, are not checked.
 * RIB_HOTKEY_MODES(name, "first", "second")
 *    A hotkey that works one of two ways, which the player chooses on its
 *    row; the author's choice is where it starts. Each way's word is the
 *    player's: hotkey-mode-<way> (words.inc).
 * RIB_HOTKEYS_SHARE(first, second)
 *    Two hotkeys that do the same inside the menu. An input may be bound to
 *    both, and there it acts once, as `first`. Every other pair holds an
 *    input once: binding it to one takes it from the other.
 * RIB_HOTKEY_BINDING(name, "prefix")
 *    A binding is its kind's prefix and what it names: a key by the name
 *    RetroArch's config gives it (input/input_key_names.inc), or the pad
 *    inputs held together, each a position of the standard pad by
 *    RetroArch's bind name for it (input_player1_<name>), joined by the
 *    chord's separator.
 * RIB_HOTKEY_PAD_CHORD("separator")
 * RIB_HOTKEY_PAD_HOME("id", "bind", "Word")
 *    The one pad input that is no position on the standard pad: each pad's
 *    own menu button, which its RetroArch profile binds to `bind`, and the
 *    word the menu and the builder show for it, whatever a pad calls it. The
 *    positions' words are the builder's (controls.json padPositions), which
 *    the exporter writes beside the bindings.
 * RIB_HOTKEY_CAPTURE_CANCEL("key")
 *    The key that ends a capture unchanged, which a capture never takes.
 * RIB_HOTKEY_CHORD(name)
 *    A hotkey that the player can also use with the fullscreen chord of the
 *    platform, Alt+Enter, or Option+Return on a Mac. We show the chord first on
 *    its row, in the words fullscreen-chord and fullscreen-chord-mac
 *    (words.inc), and the player cannot remove it. */
/* A reader defines the declarations it reads; the others read as nothing. */
#ifndef RIB_HOTKEY
#define RIB_HOTKEY(name, id, keeps, acts)
#endif
#ifndef RIB_HOTKEY_MODES
#define RIB_HOTKEY_MODES(name, first, second)
#endif
#ifndef RIB_HOTKEYS_SHARE
#define RIB_HOTKEYS_SHARE(first, second)
#endif
#ifndef RIB_HOTKEY_BINDING
#define RIB_HOTKEY_BINDING(name, prefix)
#endif
#ifndef RIB_HOTKEY_PAD_CHORD
#define RIB_HOTKEY_PAD_CHORD(separator)
#endif
#ifndef RIB_HOTKEY_PAD_HOME
#define RIB_HOTKEY_PAD_HOME(id, bind, word)
#endif
#ifndef RIB_HOTKEY_CAPTURE_CANCEL
#define RIB_HOTKEY_CAPTURE_CANCEL(key)
#endif
#ifndef RIB_HOTKEY_CHORD
#define RIB_HOTKEY_CHORD(name)
#endif

/* Opens the menu while the game runs, and inside it goes back, or closes
 * it from the pause screen. */
RIB_HOTKEY(Menu, "menu", Key, Both)
/* Presses what has focus. */
RIB_HOTKEY(Confirm, "confirm", Binding, InMenu)
/* Goes back, or closes the menu from the pause screen. */
RIB_HOTKEY(Back, "back", Binding, InMenu)
/* Saves to the slot the menu has selected, as its SAVE does. */
RIB_HOTKEY(QuickSave, "quick-save", Nothing, InGame)
/* Loads the slot the menu has selected, as its LOAD does. */
RIB_HOTKEY(QuickLoad, "quick-load", Nothing, InGame)
/* Selects the slot before the selected one, the last after the first. */
RIB_HOTKEY(PreviousSlot, "previous-slot", Nothing, InGame)
/* Selects the slot after the selected one, the first after the last. */
RIB_HOTKEY(NextSlot, "next-slot", Nothing, InGame)
/* Runs the game fast while it is held, or from one press to the next, as
 * the player chooses; offered only in a game whose author turned fast
 * forward on. */
RIB_HOTKEY(FastForward, "fast-forward", Nothing, InGame)
/* With FULLSCREEN the player switches the game's window into fullscreen and
 * back, during play and in the menu. */
RIB_HOTKEY(Fullscreen, "fullscreen", Nothing, Both)

/* Fast forward runs while held, or from one press to the next. */
RIB_HOTKEY_MODES(FastForward, "hold", "toggle")

RIB_HOTKEYS_SHARE(Menu, Back)

RIB_HOTKEY_BINDING(Key, "key:")
RIB_HOTKEY_BINDING(Pad, "pad:")
RIB_HOTKEY_PAD_CHORD("+")
RIB_HOTKEY_PAD_HOME("home", "menu_toggle", "Home")
RIB_HOTKEY_CAPTURE_CANCEL("escape")
RIB_HOTKEY_CHORD(Fullscreen)

#undef RIB_HOTKEY
#undef RIB_HOTKEY_MODES
#undef RIB_HOTKEYS_SHARE
#undef RIB_HOTKEY_BINDING
#undef RIB_HOTKEY_PAD_CHORD
#undef RIB_HOTKEY_PAD_HOME
#undef RIB_HOTKEY_CAPTURE_CANCEL
#undef RIB_HOTKEY_CHORD
