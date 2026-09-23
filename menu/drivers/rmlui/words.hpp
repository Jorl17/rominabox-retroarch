#pragma once

namespace rib::words {
/* Built-in wording only. Text from design declarations stays in the design.
 * There is no override format or localization mechanism in this table. */
inline constexpr char ContinueHint[] = "ESC  CONTINUE";
inline constexpr char BackHint[] = "ESC  BACK";
inline constexpr char CancelHint[] = "ESC  CANCEL";
inline constexpr char ChooseControl[] = "SELECT A CONTROL TO REBIND";
inline constexpr char BindingUnchanged[] = "BINDING UNCHANGED";
inline constexpr char CaptureFailed[] = "CAPTURE COULD NOT START";
inline constexpr char CaptureStarted[] = "%s: PRESS AN INPUT (10)";
inline constexpr char CaptureCountdown[] = "%s: PRESS AN INPUT (%d)";
inline constexpr char BindingConflict[] = "SAVED; ALSO USED BY %s";
inline constexpr char DefaultsLoadFailed[] = "DEFAULTS COULD NOT BE LOADED";
inline constexpr char DefaultsSaveFailed[] = "DEFAULTS RESTORED; SAVE FAILED";
inline constexpr char DefaultsRestored[] = "DEFAULTS RESTORED";
inline constexpr char BindingSaveFailed[] = "BINDING ACTIVE; SAVE FAILED";
inline constexpr char BindingSaved[] = "BINDING SAVED";
inline constexpr char CaptureTimeout[] = "TIMED OUT; BINDING UNCHANGED";
inline constexpr char PausedHeading[] = "GAME PAUSED";
inline constexpr char ControlsHeading[] = "CONTROLS";
inline constexpr char Unbound[] = "---";
inline constexpr char SlotLabel[] = "SLOT ";
inline constexpr char Occupied[] = "OCCUPIED";
inline constexpr char Empty[] = "EMPTY";
inline constexpr char SavingSlot[] = "SAVING SLOT %d...";
inline constexpr char LoadingSlot[] = "LOADING SLOT %d...";
inline constexpr char SavedSlot[] = "SLOT %d SAVED";
inline constexpr char LoadedSlot[] = "SLOT %d LOADED";
inline constexpr char SaveFailed[] = "SAVE FAILED";
inline constexpr char LoadFailed[] = "LOAD FAILED";
}
