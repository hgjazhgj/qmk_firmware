// SPDX-License-Identifier: GPL-2.0-or-later
#include "board_settings.h"
#include "eeconfig.h"

// Original manufacturer format. Explicit offsets and masks avoid depending on
// C struct padding or bitfield allocation. The other EEPROM user bytes are unused.
enum {
    SETTINGS_SIGNATURE,
    SETTINGS_SIDE_EFFECT,
    SETTINGS_SIDE_REGIONS,
    SETTINGS_SIDE_BRIGHTNESS,
    SETTINGS_SIDE_SPEED,
    SETTINGS_SIDE_RAINBOW,
    SETTINGS_SIDE_COLOUR,
    SETTINGS_DEVICE_FLAGS,
    SETTINGS_SIZE,
};

#define SETTINGS_MAGIC 0xA5
#define SETTINGS_SLEEP_ENABLE 0x01

_Static_assert(SETTINGS_SIZE == 8, "Preserve the manufacturer EEPROM format");
_Static_assert(EECONFIG_USER_DATA_SIZE >= SETTINGS_SIZE, "Board settings exceed the EEPROM user block");

static uint8_t stored[SETTINGS_SIZE];

board_side_settings_t board_settings_get_side(void) {
    return (board_side_settings_t){
        .effect     = stored[SETTINGS_SIDE_EFFECT],
        .regions    = stored[SETTINGS_SIDE_REGIONS],
        .brightness = stored[SETTINGS_SIDE_BRIGHTNESS],
        .speed      = stored[SETTINGS_SIDE_SPEED],
        .rainbow    = stored[SETTINGS_SIDE_RAINBOW],
        .colour     = stored[SETTINGS_SIDE_COLOUR],
    };
}

void board_settings_set_side(const board_side_settings_t *settings) {
    stored[SETTINGS_SIDE_EFFECT]     = settings->effect;
    stored[SETTINGS_SIDE_REGIONS]    = settings->regions;
    stored[SETTINGS_SIDE_BRIGHTNESS] = settings->brightness;
    stored[SETTINGS_SIDE_SPEED]      = settings->speed;
    stored[SETTINGS_SIDE_RAINBOW]    = settings->rainbow;
    stored[SETTINGS_SIDE_COLOUR]     = settings->colour;
}

bool board_settings_sleep_enabled(void) {
    return (stored[SETTINGS_DEVICE_FLAGS] & SETTINGS_SLEEP_ENABLE) != 0;
}

void board_settings_set_sleep_enabled(bool enabled) {
    stored[SETTINGS_DEVICE_FLAGS] = (stored[SETTINGS_DEVICE_FLAGS] & ~SETTINGS_SLEEP_ENABLE) | (enabled ? SETTINGS_SLEEP_ENABLE : 0);
}

void board_settings_save(void) {
    eeconfig_update_user_datablock(stored, 0, sizeof(stored));
}

void board_settings_reset(void) {
    const board_side_settings_t defaults = {.effect = 2, .regions = 7, .brightness = 2, .speed = 2, .rainbow = 1, .colour = 0};
    stored[SETTINGS_SIGNATURE]           = SETTINGS_MAGIC;
    board_settings_set_side(&defaults);
    board_settings_set_sleep_enabled(true);
    board_settings_save();
}

bool board_settings_load(void) {
    eeconfig_read_user_datablock(stored, 0, sizeof(stored));
    return stored[SETTINGS_SIGNATURE] == SETTINGS_MAGIC;
}
