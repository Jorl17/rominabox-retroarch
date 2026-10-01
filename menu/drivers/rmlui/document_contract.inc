/* The composed menu's contract: element name, literal id/prefix, scope,
 * presence. Grouped by scope. A screen role as the scope (Pause, Controls,
 * Options, Achievements) means the entry belongs to that screen, and Required
 * means required whenever that screen ships; Shared, Menu and Slots are
 * required in every menu document, Splash in the logo-only document.
 * Generated ids are prefixes the exporter fills in, and State classes are set
 * by the player rather than written in markup. The exporter reads this file
 * (desktop/src-tauri/src/menu/contract.rs): it writes every name it composes
 * by the name declared here, read when it compiles, and validates every
 * composed menu against this list.
 * Slots is the required slot-button prefix; kSlotCount expands it to the
 * numbered ids. */
/* A reader defines the declarations it reads; the others read as nothing. */
#ifndef RIB_SLOT_COUNT
#define RIB_SLOT_COUNT(count)
#endif
#ifndef RIB_CANVAS
#define RIB_CANVAS(width, height)
#endif
#ifndef RIB_ROLE
#define RIB_ROLE(name, word)
#endif
#ifndef RIB_ELEMENT
#define RIB_ELEMENT(name, value, scope, presence)
#endif
#ifndef RIB_CLASS
#define RIB_CLASS(name, value, scope, presence)
#endif
#ifndef RIB_ATTRIBUTE
#define RIB_ATTRIBUTE(name, value, scope, presence)
#endif
#ifndef RIB_FACT
#define RIB_FACT(name, value)
#endif
#ifndef RIB_NOTICE
#define RIB_NOTICE(name, value, hold_ms)
#endif

RIB_SLOT_COUNT(6)

/* The reference canvas, in dp: every design is laid out on it, and the
 * player scales it to fit the window, keeping its shape. The exporter gives
 * a stylesheet the same numbers as design(canvas-width) and
 * design(canvas-height). */
RIB_CANVAS(960, 600)

/* What the menu treats a screen as, when it treats it specially. design.cfg
 * names a screen's role as screen_role_<id> = "<word>", and the scopes below
 * name a role by its first field. */
RIB_ROLE(Pause, "pause")
RIB_ROLE(Options, "options")
RIB_ROLE(Controls, "controls")
RIB_ROLE(Shaders, "shaders")
RIB_ROLE(Achievements, "achievements")
RIB_ROLE(Discs, "discs")
/* QUICK SIGN IN's list of the accounts other games saved. */
RIB_ROLE(Accounts, "accounts")
/* HOTKEYS: what opens the menu, confirms and goes back in it, and saves,
 * loads and changes the slot while the game plays. */
RIB_ROLE(Hotkeys, "hotkeys")
/* UNINSTALL on Windows, RESET on a Mac: asks, then forgets the game. */
RIB_ROLE(Forget, "forget")

/* Every document. */
RIB_ELEMENT(Body, "body", Shared, Required)
RIB_ELEMENT(Screen, "screen", Shared, Required)

/* The logo-only document. */
RIB_ELEMENT(Splash, "splash", Splash, Required)
RIB_ELEMENT(SplashLogo, "splash-logo", Splash, Required)

/* Every menu document: the chrome around the screens, and what Native's
 * skeleton always draws. */
RIB_ELEMENT(Heading, "heading", Menu, Required)
RIB_ELEMENT(FooterHint, "footer-hint", Menu, Required)
RIB_ELEMENT(Footer, "footer", Menu, Optional)
RIB_ELEMENT(UnlockTitle, "unlock-title", Menu, Optional)
RIB_ELEMENT(UnlockDetail, "unlock-detail", Menu, Optional)
RIB_ELEMENT(UnlockBadge, "unlock-badge", Menu, Optional)
RIB_ELEMENT(UnlockRow, "unlock-row", Menu, Optional)
/* The notice the unlock row shows, which the player writes on it as
 * data-notice, so a design can draw each its own way: an achievement
 * unlocked, or what a hotkey did to the save slots while the game plays.
 * RIB_NOTICE(name, "value", hold_ms): how long the row holds it. An unlock
 * earned meanwhile waits for the row, so a slot notice holds it briefly. */
RIB_ATTRIBUTE(NoticeAttribute, "data-notice", State, Optional)
RIB_NOTICE(Achievement, "achievement", 4500)
RIB_NOTICE(Slot, "slot", 1500)
RIB_CLASS(MenuAction, "menu-action", Menu, Required)
RIB_CLASS(ScreenPanel, "screen-panel", Menu, Required)
RIB_CLASS(Toggle, "toggle", Menu, Optional)
/* Anything else that switches on and off when pressed, as a player setting
 * drawn as an Options entry. */
RIB_CLASS(Switch, "switch", Menu, Optional)
/* In a hint, such as the footer's "[ESC]  BACK", the key it names: the
 * words in brackets, written as their own element for a design to style. */
RIB_CLASS(HintKey, "hint-key", Menu, Optional)

/* The pause screen, and its save slots. */
RIB_ELEMENT(PausePanel, "pause-panel", Pause, Required)
RIB_ELEMENT(Status, "status", Pause, Required)
RIB_ELEMENT(Resume, "resume", Pause, Required)
RIB_ELEMENT(Save, "save", Pause, Required)
RIB_ELEMENT(Load, "load", Pause, Required)
RIB_ELEMENT(Quit, "quit", Pause, Required)
RIB_ELEMENT(Slot, "slot-", Slots, Required)
RIB_CLASS(SlotClass, "slot", Slots, Required)

/* The controls screen. Its button is on the pause row or in Options. */
RIB_ELEMENT(Controls, "controls", Controls, Required)
RIB_ELEMENT(ControlsPanel, "controls-panel", Controls, Required)
RIB_ELEMENT(ControlsBack, "controls-back", Controls, Required)
RIB_ELEMENT(ControlsReset, "controls-reset", Controls, Required)
RIB_ELEMENT(ControlsCancel, "controls-cancel", Controls, Required)
RIB_ELEMENT(ControlsStatus, "controls-status", Controls, Required)
RIB_ELEMENT(ControllerScene, "controller-scene", Controls, Required)
RIB_ELEMENT(ControlsDevice, "controls-device", Controls, Optional)
RIB_ELEMENT(ControlsDeviceCurrent, "controls-device-current", Controls, Optional)
RIB_ELEMENT(ControlsDeviceList, "controls-device-list", Controls, Optional)
RIB_CLASS(ControlGroup, "control-group", Controls, Optional)
/* A stick member's mark on the pad: an arrow for each direction, a dot for
 * the click. The one being captured is `capturing`. */
RIB_CLASS(ControlDirection, "control-direction", Controls, Optional)
RIB_CLASS(DirectionUp, "direction-up", Controls, Optional)
RIB_CLASS(DirectionRight, "direction-right", Controls, Optional)
RIB_CLASS(DirectionDown, "direction-down", Controls, Optional)
RIB_CLASS(DirectionLeft, "direction-left", Controls, Optional)
RIB_CLASS(DirectionPress, "direction-press", Controls, Optional)
RIB_CLASS(ControlCallout, "control-callout", Controls, Optional)
RIB_CLASS(ControlPickerLabel, "control-picker-label", Controls, Optional)
RIB_CLASS(ControlPickerCurrent, "control-picker-current", Controls, Optional)

/* HOTKEYS: one row for each hotkey hotkeys.inc declares, its ids the row
 * prefix and the hotkey's id. Each row has its + (the add suffix), which
 * captures a new binding, and chips numbered from 1, each showing one
 * binding, which choosing removes; as many chips as the design draws are as
 * many bindings as the row holds. The row's label (the label suffix) names
 * the hotkey wherever the menu says what happened to it.
 * A chip holding a key is marked `key`, one holding pad inputs `pad`; an
 * unused chip is hidden, and a full row's + is disabled. */
RIB_ELEMENT(HotkeysPanel, "hotkeys-panel", Hotkeys, Required)
RIB_ELEMENT(HotkeysBack, "hotkeys-back", Hotkeys, Required)
RIB_ELEMENT(HotkeysReset, "hotkeys-reset", Hotkeys, Required)
RIB_ELEMENT(HotkeysCancel, "hotkeys-cancel", Hotkeys, Required)
RIB_ELEMENT(HotkeysStatus, "hotkeys-status", Hotkeys, Required)
RIB_CLASS(HotkeyChip, "hotkey-chip", Hotkeys, Required)
RIB_CLASS(HotkeyAdd, "hotkey-add", Hotkeys, Required)
/* Where a chip's words go, when the design draws more in the chip (a
 * mark that choosing it removes it); otherwise the chip itself. */
RIB_CLASS(HotkeyWords, "hotkey-words", Hotkeys, Optional)
RIB_CLASS(ChipKey, "key", Hotkeys, Optional)
RIB_CLASS(ChipPad, "pad", Hotkeys, Optional)

/* The options screen, the volume control in it, and its entries. */
RIB_CLASS(OptionsBack, "options-back", Options, Required)
RIB_CLASS(OptionEntry, "option-entry", Options, Optional)
RIB_CLASS(Slider, "slider", Options, Optional)
RIB_CLASS(SliderTrack, "slider-track", Options, Optional)
RIB_CLASS(SliderFill, "slider-fill", Options, Optional)
RIB_CLASS(SliderThumb, "slider-thumb", Options, Optional)
RIB_CLASS(SliderReadout, "slider-readout", Options, Optional)
RIB_CLASS(VolumeArrow, "volume-arrow", Options, Optional)
RIB_CLASS(ArrowDown, "arrow-down", Options, Optional)
RIB_CLASS(ArrowUp, "arrow-up", Options, Optional)

/* The achievements screen: the account form, the session actions
 * (actions-achievements.rml) and its dialogs (dialog-achievements-*.rml). */
RIB_ELEMENT(AchievementsPanel, "achievements-panel", Achievements, Required)
RIB_ELEMENT(AchievementsList, "achievements-list", Achievements, Required)
RIB_ELEMENT(AchievementsAccount, "achievements-account", Achievements, Required)
RIB_ELEMENT(AchievementsState, "achievements-state", Achievements, Required)
RIB_ELEMENT(AchievementsSignedOut, "achievements-signed-out", Achievements, Required)
RIB_ELEMENT(AchievementsForm, "achievements-form", Achievements, Required)
RIB_ELEMENT(AchievementUsername, "achievement-username", Achievements, Required)
RIB_ELEMENT(AchievementPassword, "achievement-password", Achievements, Required)
RIB_ELEMENT(AchievementPasswordVisibility, "achievements-password-visibility", Achievements, Required)
RIB_ELEMENT(AchievementsLogin, "achievements-login", Achievements, Required)
/* QUICK SIGN IN: the button on the achievements screen that opens the saved
 * accounts, shown while any are saved, and that screen's FORGET. Both are
 * required whenever the accounts screen ships, which is with achievements. */
RIB_ELEMENT(AchievementsQuick, "achievements-quick", Accounts, Required)
RIB_ELEMENT(AccountsForget, "accounts-forget", Accounts, Required)
RIB_ELEMENT(AchievementsSubmit, "achievements-submit", Achievements, Required)
RIB_ELEMENT(AchievementsCancel, "achievements-cancel", Achievements, Required)
RIB_ELEMENT(AchievementsError, "achievements-error", Achievements, Required)
RIB_ELEMENT(AchievementsCatalog, "achievements-catalog", Achievements, Required)
RIB_ELEMENT(AchievementsSessionActions, "achievements-session-actions", Achievements, Required)
RIB_ELEMENT(AchievementsEnabled, "achievements-enabled", Achievements, Required)
RIB_ELEMENT(AchievementsSignOut, "achievements-signout", Achievements, Required)
RIB_ELEMENT(AchievementsRetry, "achievements-retry", Achievements, Required)
RIB_ELEMENT(AchievementsBack, "achievements-back", Achievements, Required)
RIB_ELEMENT(AchievementsStatus, "achievements-status", Achievements, Required)
RIB_ELEMENT(AchievementsConfirmation, "achievements-confirmation", Achievements, Required)
RIB_ELEMENT(AchievementsKeepSession, "achievements-keep-session", Achievements, Required)
RIB_ELEMENT(AchievementsEndSession, "achievements-end-session", Achievements, Required)
RIB_ELEMENT(AchievementsStartup, "achievements-startup", Achievements, Required)
RIB_ELEMENT(AchievementsStartupStatus, "achievements-startup-status", Achievements, Required)
RIB_ELEMENT(AchievementsStartupRetry, "achievements-startup-retry", Achievements, Required)
RIB_ELEMENT(AchievementsSkipStartup, "achievements-skip-startup", Achievements, Required)
RIB_ELEMENT(TextKeyboard, "text-keyboard", Achievements, Optional)
RIB_ELEMENT(TextKeyboardGrid, "text-keyboard-grid", Achievements, Optional)
RIB_CLASS(TextKey, "text-key", Achievements, Optional)

/* The forget screen's button that forgets the game and closes it: its data
 * gone, and on Windows its unpacked files, once it has closed. */
RIB_ELEMENT(ForgetConfirm, "forget-confirm", Forget, Required)
/* Its button that keeps the game and goes back. */
RIB_ELEMENT(ForgetBack, "forget-back", Forget, Required)

/* Generated list screens (shaders, achievements, the disc list). */
RIB_CLASS(List, "list", Menu, Optional)
RIB_CLASS(ListRow, "list-row", Menu, Optional)
RIB_CLASS(ListPage, "list-page", Menu, Optional)
RIB_CLASS(ListPager, "list-pager", Menu, Optional)
RIB_CLASS(ListPagerCount, "list-pager-count", Menu, Optional)
RIB_CLASS(ListPagerPrev, "list-pager-prev", Menu, Optional)
RIB_CLASS(ListPagerNext, "list-pager-next", Menu, Optional)
RIB_CLASS(ListBack, "list-back", Menu, Optional)
RIB_CLASS(ListControl, "list-control", Menu, Optional)
RIB_CLASS(ListRowTitle, "list-row-title", Menu, Optional)
RIB_CLASS(ListRowDetail, "list-row-detail", Menu, Optional)
RIB_CLASS(ListRowState, "list-row-state", Menu, Optional)
RIB_CLASS(ListRowIcon, "list-row-icon", Menu, Optional)
/* The row a live list is filled from, hidden in the list. */
RIB_CLASS(ListPrototype, "list-prototype", Menu, Optional)
/* A row with no picture and no second line. */
RIB_CLASS(Line, "line", Menu, Optional)
/* How many rows one page of a live list holds. */
RIB_ATTRIBUTE(PageSizeAttribute, "data-page-size", Menu, Optional)

/* Ids the exporter generates, by prefix. */
RIB_ELEMENT(ControlPrefix, "control-", Generated, Optional)
RIB_ELEMENT(ControlBindingPrefix, "control-binding-", Generated, Optional)
RIB_ELEMENT(ControlLabelPrefix, "control-label-", Generated, Optional)
RIB_ELEMENT(SlotImagePrefix, "slot-image-", Generated, Optional)
RIB_ELEMENT(SlotLabelPrefix, "slot-label-", Generated, Optional)
RIB_ELEMENT(SlotStatePrefix, "slot-state-", Generated, Optional)
RIB_ELEMENT(ControlsDeviceOptionPrefix, "controls-device-option-", Generated, Optional)
RIB_ELEMENT(ControlGroupPrefix, "control-group-", Generated, Optional)
RIB_ELEMENT(ControlGroupBindingPrefix, "control-group-binding-", Generated, Optional)
/* A stick member's mark, by the member's control id. */
RIB_ELEMENT(ControlDirectionPrefix, "control-direction-", Generated, Optional)
RIB_ELEMENT(AchievementRowPrefix, "achievement-", Generated, Optional)
RIB_ELEMENT(AccountRowPrefix, "account-", Generated, Optional)
/* A HOTKEYS row, by the hotkey's id: <prefix><hotkey>, and its +, label
 * and chips, <prefix><hotkey><suffix> and <prefix><hotkey>-<n>. */
RIB_ELEMENT(HotkeyPrefix, "hotkey-", Generated, Optional)
RIB_ELEMENT(AddSuffix, "-add", Generated, Optional)
RIB_ELEMENT(LabelSuffix, "-label", Generated, Optional)
/* Ids the exporter builds from a screen's or a row's own id. */
RIB_ELEMENT(ListSuffix, "-list", Generated, Optional)
RIB_ELEMENT(StatusSuffix, "-status", Generated, Optional)
RIB_ELEMENT(TitleSuffix, "-title", Generated, Optional)
RIB_ELEMENT(DetailSuffix, "-detail", Generated, Optional)
RIB_ELEMENT(StateSuffix, "-state", Generated, Optional)
RIB_ELEMENT(PageCountSuffix, "-page-count", Generated, Optional)

/* State classes the player sets. */
RIB_CLASS(Focused, "focused", State, Optional)
RIB_CLASS(Selected, "selected", State, Optional)
RIB_CLASS(Disabled, "disabled", State, Optional)
RIB_CLASS(Occupied, "occupied", State, Optional)
RIB_CLASS(Empty, "empty", State, Optional)
RIB_CLASS(On, "on", State, Optional)
RIB_CLASS(Dragging, "dragging", State, Optional)
RIB_CLASS(Capturing, "capturing", State, Optional)
RIB_CLASS(Showing, "showing", State, Optional)
RIB_CLASS(Leaving, "leaving", State, Optional)
RIB_CLASS(Overlay, "overlay", State, Optional)
/* A list row whose picture is still on its way. */
RIB_CLASS(BadgeLoading, "badge-loading", State, Optional)
/* Beside an open dialog or picker: the shared navigation part makes it no
 * stop, so the arrows stay inside. */
RIB_CLASS(NavOutside, "nav-outside", State, Optional)

/* State attributes the player sets on the document. data-focus is the id of
 * the element with focus, so a design can style anything by what has focus:
 * [data-focus=save] .slot.selected is the slot SAVE is about to use. */
RIB_ATTRIBUTE(FocusAttribute, "data-focus", State, Optional)
/* The screen a button shows, which the player writes on it. */
RIB_ATTRIBUTE(ScreenTargetAttribute, "data-screen-target", State, Optional)
/* A status line's own words, kept while the player writes a message over
 * them. */
RIB_ATTRIBUTE(PromptAttribute, "data-prompt", State, Optional)
/* Which of the on-screen keyboard's keys a button is. */
RIB_ATTRIBUTE(KeyAttribute, "data-key", State, Optional)
/* An element marked data-fact="<name>" shows that fact as its text, kept up
 * to date by the player; the design decides where. Its own text is what a
 * preview shows. The facts: */
RIB_ATTRIBUTE(FactAttribute, "data-fact", State, Optional)
/* The slot SAVE and LOAD use, as a number. */
RIB_FACT(ChosenSlotFact, "chosen-slot")
/* How many accounts QUICK SIGN IN offers, as a number. */
RIB_FACT(SavedAccountsFact, "saved-accounts")
/* The running game's shape, which the player sets on the document as its
 * width divided by its height to three decimals ("1.333" for 4:3, "1.778"
 * for 16:9), again each time the menu opens, since a game can change shape
 * while it runs. A design styles anything by it, as with data-focus. */
RIB_ATTRIBUTE(GameShapeAttribute, "data-game-shape", State, Optional)
/* An element a design marks data-game-shaped takes the game's shape: the
 * player sets its width and height, in dp, as large as the max-width and
 * max-height its stylesheet gives it allow, and nothing else. Where it sits
 * is the design's. One the stylesheet gives no largest size, as lengths,
 * is left alone. */
RIB_ATTRIBUTE(GameShapedAttribute, "data-game-shaped", Menu, Optional)

#undef RIB_SLOT_COUNT
#undef RIB_CANVAS
#undef RIB_ROLE
#undef RIB_ELEMENT
#undef RIB_CLASS
#undef RIB_ATTRIBUTE
#undef RIB_FACT
#undef RIB_NOTICE
