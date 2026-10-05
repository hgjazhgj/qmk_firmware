// Copyright 2023 @ Nuphy <https://nuphy.com/>
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

// Distinct from ChibiOS's platform-owned board.h.

#include <stdbool.h>
#include <stdint.h>

// Values shared with the Nordic protocol. State storage remains byte-sized.
enum {
    LINK_RF_24 = 0,
    LINK_BT_1,
    LINK_BT_2,
    LINK_BT_3,
    LINK_USB,
};

enum {
    RF_IDLE = 0,
    RF_PAIRING,
    RF_LINKING,
    RF_CONNECT,
    RF_DISCONNECT,
    RF_SLEEP,
    RF_SNIF,
    RF_INVALID   = 0xFE,
    RF_ERR_STATE = 0xFF,
};

enum {
    SYS_SW_WIN    = 0xA1,
    SYS_SW_MAC    = 0xA2,
    HOST_USB_TYPE = 0,
    HOST_RF_TYPE  = 2,
};

// The board selects modes; RF updates telemetry. Lighting and sleep observe it.
// This snapshot is accessed only by QMK's main task, never serialized to EEPROM.
typedef struct {
    uint8_t link_mode;
    uint8_t rf_channel;
    uint8_t ble_channel;
    uint8_t rf_state;
    uint8_t charge_state;
    uint8_t keyboard_leds;
    uint8_t battery_percent;
    uint8_t system_mode;
} device_state_t;

extern device_state_t device_state;

// All three durations use the board's 10 ms tick, not milliseconds.
#define RF_LINK_SHOW_TIME 300
#define LINK_TIMEOUT ((uint32_t)(100 * 60))
#define SLEEP_TIME_DELAY ((uint32_t)(100 * 60))

// Idle and link counters saturate at UINT16_MAX.
void     board_note_activity(void);
uint16_t board_idle_ticks(void);
bool     board_dial_is_ready(void);
uint8_t  board_host_mode(void);
void     board_set_host_mode(uint8_t mode);
void     board_release_keys(void);
