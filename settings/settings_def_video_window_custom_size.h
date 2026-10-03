/* Single-source definitions: custom window size setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* The descriptor and configuration rows are where the window size comes
 * from this setting (WINDOW_SIZE_FROM_SAVED_POSITION, configuration.h).
 * The string tables always contain this row, through the strings pass. */
#if !WINDOW_SIZE_FROM_SAVED_POSITION || defined(SETTINGS_DEF_STRINGS_PASS)
S_BOOL_EX(video_window_custom_size_enable, VIDEO_WINDOW_CUSTOM_SIZE_ENABLE,
      "video_window_custom_size_enable",
      DEFAULT_WINDOW_CUSTOM_SIZE_ENABLE, SD_FLAG_NONE, 0, CMD_EVENT_REINIT, setting_bool_action_left_with_refresh, NULL, NULL, NULL, setting_bool_action_left_with_refresh, setting_bool_action_right_with_refresh, 0,
      "Use Custom Window Size",
      "Show all content in a fixed size window of dimensions specified by 'Window Width' and 'Window Height'. When disabled, window size will be set dynamically based on 'Windowed Scale'.")
#endif
