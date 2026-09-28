/* The mouse buttons a control can be bound to: how a controls file names
 * each (input_player1_<control>_mbtn, which input_config_parse_mouse_button
 * reads and rib_host_write_bind writes), RetroArch's id for it, and how the
 * menu and the builder word it. Declared once, here: the builder reads this
 * file too (desktop/src/mouse.ts, desktop/src-tauri/src/menu/words.rs).
 *
 * RIB_MOUSE_BUTTON("value", RETRO_DEVICE_ID_MOUSE_<id>, "Word") */

RIB_MOUSE_BUTTON("1", RETRO_DEVICE_ID_MOUSE_LEFT, "Left")
RIB_MOUSE_BUTTON("2", RETRO_DEVICE_ID_MOUSE_RIGHT, "Right")
RIB_MOUSE_BUTTON("3", RETRO_DEVICE_ID_MOUSE_MIDDLE, "Middle")
RIB_MOUSE_BUTTON("4", RETRO_DEVICE_ID_MOUSE_BUTTON_4, "Button 4")
RIB_MOUSE_BUTTON("5", RETRO_DEVICE_ID_MOUSE_BUTTON_5, "Button 5")
RIB_MOUSE_BUTTON("wu", RETRO_DEVICE_ID_MOUSE_WHEELUP, "Wheel up")
RIB_MOUSE_BUTTON("wd", RETRO_DEVICE_ID_MOUSE_WHEELDOWN, "Wheel down")
RIB_MOUSE_BUTTON("whu", RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELUP, "Wheel left")
RIB_MOUSE_BUTTON("whd", RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELDOWN, "Wheel right")

#undef RIB_MOUSE_BUTTON
