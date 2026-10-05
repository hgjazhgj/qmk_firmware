// Copyright 2023 @ Nuphy <https://nuphy.com/>
// SPDX-License-Identifier: GPL-2.0-or-later

// Shared initializer for the default and VIA factory keymaps.
// Keep layer numbers stable: they are also used by saved VIA keymaps and mode switching.
// Include this file inside a keymaps[] initializer, after QMK_KEYBOARD_H.
// clang-format off
    // Mac
    [HALO75_MAC] = LAYOUT(
        KC_ESC,     KC_SCRL,    KC_BRK,     KC_MCTL,    MAC_SEARCH, MAC_VOICE,  MAC_DND,    KC_MPRV,    KC_MPLY,    KC_MNXT,    KC_MUTE,    KC_VOLD,    KC_VOLU,    MAC_PRTA,   KC_INS,     KC_DEL,
        KC_GRV,     KC_1,       KC_2,       KC_3,       KC_4,       KC_5,       KC_6,       KC_7,       KC_8,       KC_9,       KC_0,       KC_MINS,    KC_EQL,     KC_BSPC,    KC_HOME,
        KC_TAB,     KC_Q,       KC_W,       KC_E,       KC_R,       KC_T,       KC_Y,       KC_U,       KC_I,       KC_O,       KC_P,       KC_LBRC,    KC_RBRC,    KC_BSLS,    KC_END,
        KC_CAPS,    KC_A,       KC_S,       KC_D,       KC_F,       KC_G,       KC_H,       KC_J,       KC_K,       KC_L,       KC_SCLN,    KC_QUOT,    KC_ENT,     KC_PGUP,
        KC_LSFT,    KC_Z,       KC_X,       KC_C,       KC_V,       KC_B,       KC_N,       KC_M,       KC_COMM,    KC_DOT,     KC_SLSH,    KC_RSFT,    KC_UP,      KC_PGDN,
        KC_LCTL,    KC_LOPT,    KC_LCMD,    KC_SPC,     MO(HALO75_MAC_FN), KC_RCTL,    KC_LEFT,    KC_DOWN,    KC_RIGHT),
    // Mac Fn
    [HALO75_MAC_FN] = LAYOUT(
        BAT_SHOW,   KC_F1,      KC_F2,      KC_F3,      KC_F4,      KC_F5,      KC_F6,      KC_F7,      KC_F8,      KC_F9,      KC_F10,     KC_F11,     KC_F12,     MAC_PRT,    _______,    SLEEP_MODE,
        KC_NUM,     KC_P1,      KC_P2,      KC_P3,      KC_P4,      KC_P5,      KC_P6,      KC_P7,      KC_P8,      KC_P9,      KC_P0,      KC_PMNS,    KC_PPLS,    _______,    RM_PREV,
        _______,    LNK_RF,     LNK_BLE1,   LNK_BLE2,   LNK_BLE3,   LNK_USB,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    RM_TOGG,    RM_NEXT,
        _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    KC_PENT,    RM_HUEU,
        _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    KC_PAST,    KC_PDOT,    KC_PSLS,    _______,    RM_VALU,    RM_HUED,
        _______,    KC_ROPT,    KC_RCMD,    _______,    MO(HALO75_WIN_FN), MO(HALO75_SIDE), RM_SPDD,    RM_VALD,    RM_SPDU),
    // Windows
    [HALO75_WIN] = LAYOUT(
        KC_ESC,     KC_F1,      KC_F2,      KC_F3,      KC_F4,      KC_F5,      KC_F6,      KC_F7,      KC_F8,      KC_F9,      KC_F10,     KC_F11,     KC_F12,     KC_PSCR,    KC_INS,     KC_DEL,
        KC_GRV,     KC_1,       KC_2,       KC_3,       KC_4,       KC_5,       KC_6,       KC_7,       KC_8,       KC_9,       KC_0,       KC_MINS,    KC_EQL,     KC_BSPC,    KC_HOME,
        KC_TAB,     KC_Q,       KC_W,       KC_E,       KC_R,       KC_T,       KC_Y,       KC_U,       KC_I,       KC_O,       KC_P,       KC_LBRC,    KC_RBRC,    KC_BSLS,    KC_END,
        KC_CAPS,    KC_A,       KC_S,       KC_D,       KC_F,       KC_G,       KC_H,       KC_J,       KC_K,       KC_L,       KC_SCLN,    KC_QUOT,    KC_ENT,     KC_PGUP,
        KC_LSFT,    KC_Z,       KC_X,       KC_C,       KC_V,       KC_B,       KC_N,       KC_M,       KC_COMM,    KC_DOT,     KC_SLSH,    KC_RSFT,    KC_UP,      KC_PGDN,
        KC_LCTL,    KC_LWIN,    KC_LALT,    KC_SPC,     MO(HALO75_WIN_FN), KC_RCTL,    KC_LEFT,    KC_DOWN,    KC_RIGHT),
    // Windows Fn
    [HALO75_WIN_FN] = LAYOUT(
        BAT_SHOW,   KC_BRID,    KC_BRIU,    MS_BTN1,    MS_BTN2,    MS_BTN3,    KC_MSTP,    KC_MPRV,    KC_MPLY,    KC_MNXT,    KC_MUTE,    KC_VOLD,    KC_VOLU,    KC_SCRL,    KC_BRK,     SLEEP_MODE,
        KC_NUM,     KC_P1,      KC_P2,      KC_P3,      KC_P4,      KC_P5,      KC_P6,      KC_P7,      KC_P8,      KC_P9,      KC_P0,      KC_PMNS,    KC_PPLS,    _______,    RM_PREV,
        _______,    LNK_RF,     LNK_BLE1,   LNK_BLE2,   LNK_BLE3,   LNK_USB,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    RM_TOGG,    RM_NEXT,
        _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    KC_PENT,    RM_HUEU,
        _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    KC_PAST,    KC_PDOT,    KC_PSLS,    _______,    RM_VALU,    RM_HUED,
        _______,    KC_RWIN,    KC_RALT,    _______,    MO(HALO75_WIN_FN), MO(HALO75_SIDE), RM_SPDD,    RM_VALD,    RM_SPDU),
    // Side-light controls
    [HALO75_SIDE] = LAYOUT(
        _______,    KC_F13,     KC_F14,     KC_F15,     KC_F16,     KC_F17,     KC_F18,     KC_F19,     KC_F20,     KC_F21,     KC_F22,     KC_F23,     KC_F24,     _______,    _______,    _______,
        _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    DEV_RESET,  _______,
        _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,
        _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    SIDE_MOD_A,
        _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    SIDE_HUD,   SIDE_HUI,   _______,    _______,    SIDE_VAI,   SIDE_MOD_B,
        _______,    GU_TOGG,    _______,    _______,    MO(HALO75_WIN_FN), MO(HALO75_SIDE), SIDE_SPD,   SIDE_VAD,   SIDE_SPI),
// clang-format on
