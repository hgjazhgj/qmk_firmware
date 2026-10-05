// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <stdbool.h>
#include <stdint.h>

// Value object shared with the side-light module; never written as a C struct.
typedef struct {
    uint8_t effect;
    uint8_t regions;
    uint8_t brightness;
    uint8_t speed;
    uint8_t rainbow;
    uint8_t colour;
} board_side_settings_t;

// Load once at startup. On a missing signature, the caller initializes its QMK
// RGB settings before calling board_settings_reset() to save the defaults.
bool board_settings_load(void);
// Restore known preferences and save once; keep reserved device-flag bits.
void board_settings_reset(void);

// Accessors modify only the in-memory preferences. In particular, toggling sleep
// remains deferred until the next explicit save, as in the original firmware.
board_side_settings_t board_settings_get_side(void);
void                  board_settings_set_side(const board_side_settings_t *settings);
bool                  board_settings_sleep_enabled(void);
void                  board_settings_set_sleep_enabled(bool enabled);
void                  board_settings_save(void);
