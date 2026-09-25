/* The composed menu's contract: element name, literal id/prefix, scope,
 * presence. Grouped by scope. A screen role as the scope (Pause, Controls,
 * Options, Achievements) means the entry belongs to that screen, and Required
 * means required whenever that screen ships; Shared, Menu and Slots are
 * required in every menu document, Splash in the logo-only document.
 * Generated ids are prefixes the exporter fills in, and State classes are set
 * by the player rather than written in markup. The exporter validates every
 * composed menu against this list (desktop/src-tauri/src/menu/contract.rs).
 * Slots is the required slot-button prefix; kSlotCount expands it to the
 * numbered ids. */
RIB_SLOT_COUNT(6)

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
RIB_ELEMENT(UnlockTitle, "unlock-title", Menu, Optional)
RIB_ELEMENT(UnlockDetail, "unlock-detail", Menu, Optional)
RIB_ELEMENT(UnlockBadge, "unlock-badge", Menu, Optional)
RIB_ELEMENT(UnlockRow, "unlock-row", Menu, Optional)
RIB_CLASS(MenuAction, "menu-action", Menu, Required)
RIB_CLASS(ScreenPanel, "screen-panel", Menu, Required)
RIB_CLASS(Toggle, "toggle", Menu, Optional)

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

/* Generated list screens (shaders, achievements, the disc list). */
RIB_CLASS(List, "list", Menu, Optional)
RIB_CLASS(ListRow, "list-row", Menu, Optional)
RIB_CLASS(ListPage, "list-page", Menu, Optional)
RIB_CLASS(ListPager, "list-pager", Menu, Optional)
RIB_CLASS(ListPagerCount, "list-pager-count", Menu, Optional)
RIB_CLASS(ListPagerPrev, "list-pager-prev", Menu, Optional)
RIB_CLASS(ListPagerNext, "list-pager-next", Menu, Optional)
RIB_CLASS(ListToggle, "list-toggle", Menu, Optional)
RIB_CLASS(ListBack, "list-back", Menu, Optional)
RIB_CLASS(ListControl, "list-control", Menu, Optional)
RIB_CLASS(ListRowTitle, "list-row-title", Menu, Optional)
RIB_CLASS(ListRowDetail, "list-row-detail", Menu, Optional)
RIB_CLASS(ListRowState, "list-row-state", Menu, Optional)

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

/* State attributes the player sets on the document. data-focus is the id of
 * the element with focus, so a design can style anything by what has focus:
 * [data-focus=save] .slot.selected is the slot SAVE is about to use. */
RIB_ATTRIBUTE(FocusAttribute, "data-focus", State, Optional)
/* An element marked data-fact="<name>" shows that fact as its text, kept up
 * to date by the player; the design decides where. Its own text is what a
 * preview shows. The facts: */
RIB_ATTRIBUTE(FactAttribute, "data-fact", State, Optional)
/* The slot SAVE and LOAD use, as a number. */
RIB_FACT(ChosenSlotFact, "chosen-slot")
/* How many accounts QUICK SIGN IN offers, as a number. */
RIB_FACT(SavedAccountsFact, "saved-accounts")
