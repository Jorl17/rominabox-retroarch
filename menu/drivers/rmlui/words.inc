/* The words the menu writes itself, over whatever the design's markup says:
 * statuses, the words on slots and lists, and the words composition writes
 * for the player's settings. A status line's prompt is the design's own
 * text, which a status covers until it lapses. A design gives its own
 * wording in design.json "words", by the id below; the exporter writes each
 * to design.cfg as word_<id>, and a word a design does not give is the
 * English here. {name} is a value the menu puts in: a design's words may use
 * the ones the English uses, and no others. Neither holds a double quote.
 *
 * RIB_WORD(name, id, English) */

/* The footer while a binding is captured. A screen's own heading and footer
 * are its design.json "screens" entry's. */
RIB_WORD(CancelHint, "cancel-hint", "[ESC]  CANCEL")

/* DATA's statuses, and the words in its dialog before an import. {game}
 * is the title of the game the zip's data is from. */
RIB_WORD(DataExported, "data-exported", "DATA EXPORTED")
RIB_WORD(DataExportFailed, "data-export-failed", "EXPORT FAILED")
RIB_WORD(DataImportFailed, "data-import-failed", "IMPORT FAILED")
RIB_WORD(DataImportSame, "data-import-same", "IMPORTING THIS DATA WILL PERMANENTLY REPLACE YOUR SAVES, STATES AND SETTINGS. THE GAME WILL THEN IMMEDIATELY RESTART.")
RIB_WORD(DataImportOther, "data-import-other", "THIS DATA IS FROM {game}. IMPORTING IT WILL PERMANENTLY REPLACE YOUR SAVES, STATES AND SETTINGS. THE GAME WILL THEN IMMEDIATELY RESTART.")

/* The pad screen's statuses. */
RIB_WORD(CaptureCountdown, "capture-countdown", "{control}: PRESS AN INPUT ({seconds})")
RIB_WORD(CaptureFailed, "capture-failed", "CAPTURE COULD NOT START")
RIB_WORD(BindingSaved, "binding-saved", "BINDING SAVED")
RIB_WORD(BindingConflict, "binding-conflict", "SAVED; ALSO USED BY {control}")
RIB_WORD(BindingSaveFailed, "binding-save-failed", "BINDING ACTIVE; SAVE FAILED")
RIB_WORD(BindingUnchanged, "binding-unchanged", "BINDING UNCHANGED")
RIB_WORD(CaptureTimeout, "capture-timeout", "TIMED OUT; BINDING UNCHANGED")
RIB_WORD(DefaultsRestored, "defaults-restored", "DEFAULTS RESTORED")
RIB_WORD(DefaultsSaveFailed, "defaults-save-failed", "DEFAULTS RESTORED; SAVE FAILED")
RIB_WORD(DefaultsLoadFailed, "defaults-load-failed", "DEFAULTS COULD NOT BE LOADED")
RIB_WORD(Unbound, "unbound", "---")

/* HOTKEYS' statuses, beside the pad screen's capture words, which it
 * shares. {control} is a hotkey as its row names it. */
RIB_WORD(BindingRemoved, "binding-removed", "BINDING REMOVED")
RIB_WORD(BindingSwapped, "binding-swapped", "SWAPPED WITH {control}")
RIB_WORD(BindingMoved, "binding-moved", "TAKEN FROM {control}")
RIB_WORD(BindingNoRoom, "binding-no-room", "NO ROOM IN {control}")
RIB_WORD(BindingNeeded, "binding-needed", "{control} MUST KEEP A BINDING")
RIB_WORD(KeyNeeded, "key-needed", "{control} MUST KEEP A KEY")
RIB_WORD(BindingUnusable, "binding-unusable", "USE A KEY OR A PAD BUTTON")
/* A hotkey that acts while the game plays refused what the game reads:
 * {control} is the game's control, as CONTROLS shows it. */
RIB_WORD(GameInput, "game-input", "THE GAME USES THAT FOR {control}")
/* The ways a hotkey with ways works (hotkeys.inc), on its row, and what a
 * change of way says. */
RIB_WORD(HotkeyModeHold, "hotkey-mode-hold", "HOLD")
RIB_WORD(HotkeyModeToggle, "hotkey-mode-toggle", "TOGGLE")
RIB_WORD(HotkeyModeSaved, "hotkey-mode-saved", "{control}: {mode}")
/* The fullscreen chord, first on the row of a hotkey that has it
 * (hotkeys.inc), in a game for Windows and in a game for a Mac. */
RIB_WORD(FullscreenChord, "fullscreen-chord", "ALT+ENTER")
RIB_WORD(FullscreenChordMac, "fullscreen-chord-mac", "OPTION+RETURN")

/* The save slots and what saving and loading say. */
RIB_WORD(Slot, "slot", "SLOT {slot}")
RIB_WORD(Occupied, "occupied", "OCCUPIED")
RIB_WORD(Empty, "empty", "EMPTY")
RIB_WORD(SavingSlot, "saving-slot", "SAVING SLOT {slot}...")
RIB_WORD(LoadingSlot, "loading-slot", "LOADING SLOT {slot}...")
RIB_WORD(SavedSlot, "saved-slot", "SLOT {slot} SAVED")
RIB_WORD(LoadedSlot, "loaded-slot", "SLOT {slot} LOADED")
RIB_WORD(SaveFailed, "save-failed", "SAVE FAILED")
RIB_WORD(LoadFailed, "load-failed", "LOAD FAILED")
/* What a hotkey did while the game plays, in the notice row; a save or load
 * that failed says SAVE FAILED or LOAD FAILED there. */
RIB_WORD(QuickSaved, "quick-saved", "SAVED TO SLOT {slot}")
RIB_WORD(QuickLoaded, "quick-loaded", "LOADED SLOT {slot}")
RIB_WORD(QuickSlotEmpty, "quick-slot-empty", "SLOT {slot} IS EMPTY")
RIB_WORD(QuickSlot, "quick-slot", "SLOT {slot}")

/* Lists: a pager's count, the disc list, QUICK SIGN IN's accounts. */
RIB_WORD(PageCount, "page-count", "{page}/{pages}")
RIB_WORD(Disc, "disc", "Disc {number}")
RIB_WORD(ShowingDiscs, "showing-discs", "SHOWING {shown} OF {count}")
RIB_WORD(QuickSignInFailed, "quick-sign-in-failed", "COULD NOT USE THIS ACCOUNT")
/* A list's mark on its current row: the filter that is running, the disc in
 * the tray. */
RIB_WORD(ShaderMark, "shader-mark", "ON")
RIB_WORD(DiscMark, "disc-mark", "IN")

/* The player's settings in Options. Composition writes a setting's name and
 * a level's ends into the document; the menu keeps a switch's state current. */
RIB_WORD(Volume, "volume", "VOLUME")
RIB_WORD(PlayInBackground, "play-in-background", "PLAY IN BACKGROUND")
RIB_WORD(Rumble, "rumble", "RUMBLE")
RIB_WORD(Brightness, "brightness", "BRIGHTNESS")
RIB_WORD(Contrast, "contrast", "CONTRAST")
RIB_WORD(FastForwardSpeed, "fast-forward-speed", "FAST FORWARD")
RIB_WORD(LevelLow, "level-low", "LOW")
RIB_WORD(LevelHigh, "level-high", "HIGH")
RIB_WORD(FastForwardSpeed1, "fast-forward-speed-1", "2X")
RIB_WORD(FastForwardSpeed2, "fast-forward-speed-2", "4X")
RIB_WORD(FastForwardSpeed3, "fast-forward-speed-3", "MAX")
RIB_WORD(SwitchOn, "switch-on", "ON")
RIB_WORD(SwitchOff, "switch-off", "OFF")

/* The achievements screen and its dialogs. */
RIB_WORD(NoAccount, "no-account", "RETROACHIEVEMENTS")
RIB_WORD(AchievementsOn, "achievements-on", "ON")
RIB_WORD(AchievementsConnecting, "achievements-connecting", "CONNECTING")
RIB_WORD(AchievementsOff, "achievements-off", "OFF")
RIB_WORD(TurnOn, "turn-on", "TURN ON")
RIB_WORD(TurnOff, "turn-off", "TURN OFF")
RIB_WORD(CancelConnecting, "cancel-connecting", "CANCEL")
RIB_WORD(SigningIn, "signing-in", "Signing in...")
RIB_WORD(LoadingAchievements, "loading-achievements", "Loading achievements...")
RIB_WORD(WaitingToSync, "waiting-to-sync", "Earned achievements waiting to sync. Keep the game open.")
RIB_WORD(AchievementsTurnedOff, "achievements-turned-off", "Achievements are off.")
RIB_WORD(EnterDetails, "enter-details", "Enter your username and password.")
RIB_WORD(SignInNotStarted, "sign-in-not-started", "Could not start sign in. Check your details and try again.")
RIB_WORD(ShowPassword, "show-password", "SHOW")
RIB_WORD(HidePassword, "hide-password", "HIDE")
RIB_WORD(QuitAnyway, "quit-anyway", "QUIT ANYWAY")
RIB_WORD(SignOutAnyway, "sign-out-anyway", "SIGN OUT ANYWAY")
RIB_WORD(RestoringGame, "restoring-game", "Restoring your game...")
RIB_WORD(Connecting, "connecting", "Connecting to RetroAchievements...")
RIB_WORD(Locked, "locked", "LOCKED")
RIB_WORD(Earned, "earned", "EARNED")
RIB_WORD(Syncing, "syncing", "SYNCING")
RIB_WORD(Unsupported, "unsupported", "UNSUPPORTED")
RIB_WORD(AchievementPoints, "achievement-points", "{points} PT / {state}")
RIB_WORD(UnlockPoints, "unlock-points", "{points} points")

/* The on-screen keyboard's own keys. */
RIB_WORD(KeyDelete, "key-delete", "DEL")
RIB_WORD(KeyEnter, "key-enter", "OK")
RIB_WORD(KeyUpper, "key-upper", "ABC")
RIB_WORD(KeyLower, "key-lower", "abc")
RIB_WORD(KeySymbols, "key-symbols", "#+=")
RIB_WORD(KeySpace, "key-space", "_")

#undef RIB_WORD
