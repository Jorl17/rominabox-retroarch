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

/* MENU CONTROLS' statuses, beside the pad screen's capture words, which it
 * shares. {control} is an action as its row names it. */
RIB_WORD(BindingRemoved, "binding-removed", "BINDING REMOVED")
RIB_WORD(BindingSwapped, "binding-swapped", "SWAPPED WITH {control}")
RIB_WORD(BindingMoved, "binding-moved", "TAKEN FROM {control}")
RIB_WORD(BindingNoRoom, "binding-no-room", "NO ROOM IN {control}")
RIB_WORD(BindingNeeded, "binding-needed", "{control} MUST KEEP A BINDING")
RIB_WORD(KeyNeeded, "key-needed", "{control} MUST KEEP A KEY")
RIB_WORD(BindingUnusable, "binding-unusable", "USE A KEY OR A PAD BUTTON")

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
RIB_WORD(LevelLow, "level-low", "LOW")
RIB_WORD(LevelHigh, "level-high", "HIGH")
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
