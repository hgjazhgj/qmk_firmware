// Copyright 2023 Persama (@Persama)
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "quantum.h"

// Public keymap interface. Keep these values stable for existing VIA layouts.
enum custom_keycodes {
    RF_DFU = QK_KB_0,
    LNK_USB,
    LNK_RF,
    LNK_BLE1,
    LNK_BLE2,
    LNK_BLE3,

    MAC_SEARCH,
    MAC_VOICE,
    MAC_DND,
    MAC_PRT,
    MAC_PRTA,

    DEV_RESET,
    SLEEP_MODE,
    BAT_SHOW,

    SIDE_VAI,
    SIDE_VAD,
    SIDE_MOD_A,
    SIDE_MOD_B,
    SIDE_HUI,
    SIDE_HUD,
    SIDE_SPI,
    SIDE_SPD,
};

enum halo75_layers {
    HALO75_MAC = 0,
    HALO75_MAC_FN,
    HALO75_WIN,
    HALO75_WIN_FN,
    HALO75_SIDE,
};
