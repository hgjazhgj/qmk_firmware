// Copyright 2023 @ Nuphy <https://nuphy.com/>
// SPDX-License-Identifier: GPL-2.0-or-later

#include QMK_KEYBOARD_H

// Replace this initializer for a custom layout; keymap-specific hooks can follow it.
const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
#include "../factory_keymap.inc"
};
