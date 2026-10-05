/*
Copyright 2023 @ Nuphy <https://nuphy.com/>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "ansi.h"
#include "board_runtime.h"
#include "board_settings.h"
#include "held_key.h"
#include "rf.h"
#include "rf_reports.h"
#include "rf_driver.h"
#include "side.h"
#include "sleep.h"

// Runtime status shared by board modules; settings storage is private.
device_state_t device_state = {
    .battery_percent = 100,
    .link_mode       = LINK_USB,
    .rf_state        = RF_IDLE,
};

enum { BOARD_LONG_PRESS_MS = 3000 };

typedef struct {
    struct {
        host_driver_t *usb_driver;
        uint8_t        mode;
    } host;
    struct {
        uint32_t timer;
        uint8_t  previous;
        uint8_t  debounce;
        bool     power_on;
        bool     ready;
    } dial;
    struct {
        uint32_t   timer;
        held_key_t channel;
        held_key_t reset;
    } keys;
    struct {
        uint8_t saved_brightness;
        uint8_t mode_before_battery;
    } rgb;
    uint32_t tick_timer;
    uint16_t idle_ticks;
    bool     first_tick;
    bool     windows_locked;
} board_context_t;

static board_context_t board = {
    .dial       = {.previous = 0xf0, .power_on = true},
    .first_tick = true,
};

void board_note_activity(void) {
    board.idle_ticks = 0;
}

uint16_t board_idle_ticks(void) {
    return board.idle_ticks;
}

bool board_dial_is_ready(void) {
    return board.dial.ready;
}

uint8_t board_host_mode(void) {
    return board.host.mode;
}

void board_set_host_mode(uint8_t mode) {
    board.host.mode = mode;
    host_set_driver(mode == HOST_USB_TYPE ? board.host.usb_driver : &rf_host_driver);
}

/**
 * @brief  gpio initial.
 */
void keyboard_pre_init_kb(void) {
    // QMK initializes the IS31FL3733 drivers before keyboard_post_init_kb.
    // Supply power and release shutdown before the first I2C transaction.
    gpio_set_pin_output(DC_BOOST_PIN);
    gpio_write_pin_high(DC_BOOST_PIN);

    // Initializes the RGB Driver SDB pin
    gpio_set_pin_output(RGB_DRIVER_SDB1);
    gpio_write_pin_high(RGB_DRIVER_SDB1);
    gpio_set_pin_output(RGB_DRIVER_SDB2);
    gpio_write_pin_high(RGB_DRIVER_SDB2);

    keyboard_pre_init_user();
}

static void board_init_pins(void) {
    // RF wake up pin configuration
    gpio_set_pin_output(NRF_WAKEUP_PIN);
    gpio_write_pin_high(NRF_WAKEUP_PIN);

    // RFboot Control pin
    gpio_set_pin_input_high(NRF_BOOT_PIN);

    // RF reset pin configuration
    gpio_set_pin_output(NRF_RESET_PIN);
    gpio_write_pin_low(NRF_RESET_PIN);
    wait_ms(50);
    gpio_write_pin_high(NRF_RESET_PIN);

    // Switch detection pin
    gpio_set_pin_input_high(DEV_MODE_PIN);
    gpio_set_pin_input_high(SYS_MODE_PIN);
}

static void board_select_channel(uint8_t channel) {
    device_state.link_mode   = channel;
    device_state.rf_channel  = channel;
    device_state.ble_channel = channel;
}

static void board_pair_channel(uint8_t channel) {
    board_select_channel(channel);
    for (uint8_t attempt = 0; attempt < 5; ++attempt) {
        uart_send_cmd(CMD_NEW_ADV, 0, 1);
        wait_ms(20);
        rf_receive_task();
        if (rf_pairing_acknowledged()) break;
    }
}

static void board_factory_reset(void) {
    if (device_state.link_mode != LINK_USB) {
        if (device_state.link_mode != LINK_RF_24) {
            board_select_channel(LINK_BT_1);
        }
    } else {
        device_state.ble_channel = LINK_BT_1;
    }

    uart_send_cmd(CMD_SET_LINK, 10, 10);
    wait_ms(500);
    uart_send_cmd(CMD_CLR_DEVICE, 10, 10);

    eeconfig_init();
    side_show_reset();
    side_reset_settings();

    keymap_config.no_gui = false;
    board.windows_locked = false;
    bool mac             = device_state.system_mode == SYS_SW_MAC;
    default_layer_set(1UL << (mac ? HALO75_MAC : HALO75_WIN));
    keymap_config.nkro = !mac;
}

static void board_scan_held_keys(void) {
    if (timer_elapsed32(board.keys.timer) < 100) return;
    board.keys.timer = timer_read32();

    if (held_key_poll(&board.keys.channel, timer_read32(), BOARD_LONG_PRESS_MS) == HELD_KEY_LONG) {
        board_pair_channel(board.keys.channel.identity);
    }
    if (held_key_poll(&board.keys.reset, timer_read32(), BOARD_LONG_PRESS_MS) == HELD_KEY_LONG) {
        board_factory_reset();
    }
}
/**
 * @brief  Release all keys, clear keyboard report.
 */
void board_release_keys(void) {
    uint8_t report_buf[16];
    bool    nkro_temp = keymap_config.nkro;

    clear_weak_mods();
    clear_mods();
    clear_keyboard();

    keymap_config.nkro = 1;
    memset(nkro_report, 0, sizeof(report_nkro_t));
    host_nkro_send(nkro_report);
    wait_ms(10);

    keymap_config.nkro = 0;
    memset(keyboard_report, 0, sizeof(report_keyboard_t));
    host_keyboard_send(keyboard_report);
    wait_ms(10);

    keymap_config.nkro = nkro_temp;

    if (device_state.link_mode != LINK_USB) {
        memset(report_buf, 0, 16);
        uart_send_report(CMD_RPT_BIT_KB, report_buf, 16);
        wait_ms(10);
        uart_send_report(CMD_RPT_BYTE_KB, report_buf, 8);
        wait_ms(10);
    }

    rf_clear_keyboard_reports();
}

/**
 * @brief  switch device link mode.
 * @param mode : link mode
 */
static void switch_dev_link(uint8_t mode) {
    if (mode > LINK_USB) return;
    board_release_keys();

    device_state.link_mode = mode;
    device_state.rf_state  = RF_IDLE;
    rf_request_channel_update();

    if (mode == LINK_USB) {
        board_set_host_mode(HOST_USB_TYPE);
        side_restart_link_indicator();
    } else {
        board_set_host_mode(HOST_RF_TYPE);
    }
}

/**
 * @brief  scan dial switch.
 */
static void board_scan_switches(void) {
    uint8_t dial_scan = 0;

    if (!board.dial.power_on) {
        if (timer_elapsed32(board.dial.timer) < 20) return;
    }
    board.dial.timer = timer_read32();

    gpio_set_pin_input_high(DEV_MODE_PIN);
    gpio_set_pin_input_high(SYS_MODE_PIN);

    if (gpio_read_pin(DEV_MODE_PIN)) dial_scan |= 0X01;
    if (gpio_read_pin(SYS_MODE_PIN)) dial_scan |= 0X02;

    if (board.dial.previous != dial_scan) {
        board_release_keys();
        board.dial.previous = dial_scan;
        board_note_activity();
        rf_reset_link_timer();
        board.dial.debounce = 25;
        board.dial.ready    = 0;
        return;
    } else if (board.dial.debounce) {
        board.dial.debounce--;
        return;
    }

    if (dial_scan & 0x01) {
        if (device_state.link_mode != LINK_USB) {
            switch_dev_link(LINK_USB);
        }
    } else {
        if (device_state.link_mode != device_state.rf_channel) {
            switch_dev_link(device_state.rf_channel);
        }
    }

    if (dial_scan & 0x02) {
        if (device_state.system_mode != SYS_SW_WIN) {
            side_notify_system_change();
            default_layer_set(1UL << HALO75_WIN);
            device_state.system_mode = SYS_SW_WIN;
            keymap_config.no_gui     = board.windows_locked;
            board_release_keys();
        }
        keymap_config.nkro = 1;
    } else {
        if (device_state.system_mode != SYS_SW_MAC) {
            side_notify_system_change();
            default_layer_set(1UL << HALO75_MAC);
            device_state.system_mode = SYS_SW_MAC;
            board.windows_locked     = keymap_config.no_gui;
            board_release_keys();
        }
        keymap_config.nkro   = 0;
        keymap_config.no_gui = 0;
    }

    if (board.dial.ready == 0) {
        board.dial.ready    = 1;
        board.dial.power_on = 0;

        if (device_state.link_mode != LINK_USB) {
            host_set_driver(&rf_host_driver);
        }
    }
}

/**
 * @brief  power on scan dial switch.
 */
static void board_init_switches(void) {
    uint8_t dial_scan_dev  = 0;
    uint8_t dial_scan_sys  = 0;
    uint8_t dial_check_dev = 0;
    uint8_t dial_check_sys = 0;
    uint8_t debounce       = 0;

    board.windows_locked = 0;

    gpio_set_pin_input_high(DEV_MODE_PIN);
    gpio_set_pin_input_high(SYS_MODE_PIN);

    for (debounce = 0; debounce < 10; debounce++) {
        dial_scan_dev = 0;
        dial_scan_sys = 0;
        if (gpio_read_pin(DEV_MODE_PIN))
            dial_scan_dev = 0x01;
        else
            dial_scan_dev = 0;
        if (gpio_read_pin(SYS_MODE_PIN))
            dial_scan_sys = 0x01;
        else
            dial_scan_sys = 0;
        if ((dial_scan_dev != dial_check_dev) || (dial_scan_sys != dial_check_sys)) {
            dial_check_dev = dial_scan_dev;
            dial_check_sys = dial_scan_sys;
            debounce       = 0;
        }
        wait_ms(1);
    }
    if (dial_scan_dev) {
        if (device_state.link_mode != LINK_USB) {
            switch_dev_link(LINK_USB);
        }
    } else {
        if (device_state.link_mode != device_state.rf_channel) {
            switch_dev_link(device_state.rf_channel);
        }
    }
    // WIN/MAC
    if (dial_scan_sys) {
        if (device_state.system_mode != SYS_SW_WIN) {
            default_layer_set(1UL << HALO75_WIN); // WIN
            device_state.system_mode = SYS_SW_WIN;
            keymap_config.nkro       = 1;
            board_release_keys();
        }
    } else {
        if (device_state.system_mode != SYS_SW_MAC) {
            default_layer_set(1UL << HALO75_MAC); // MAC
            device_state.system_mode = SYS_SW_MAC;
            keymap_config.nkro       = 0;
            board.windows_locked     = keymap_config.no_gui;
            keymap_config.no_gui     = 0;
            board_release_keys();
        }
    }
}

// Each press owns its timestamp and channel; a different key's release is ignored.
static void process_channel_key(uint8_t channel, bool pressed) {
    if (pressed) {
        if (device_state.link_mode != LINK_USB) {
            held_key_start(&board.keys.channel, channel, timer_read32());
            board_release_keys();
        }
        return;
    }

    held_key_result_t result = held_key_release(&board.keys.channel, channel, timer_read32(), BOARD_LONG_PRESS_MS);
    if (result == HELD_KEY_SHORT) {
        board_select_channel(channel);
        uart_send_cmd(CMD_SET_LINK, 10, 20);
    } else if (result == HELD_KEY_LONG) {
        // Release can arrive after the deadline but before the next 100 ms scan.
        board_pair_channel(channel);
    }
}
/**
 * @brief  qmk process record
 */
bool process_record_kb(uint16_t keycode, keyrecord_t *record) {
    // A key consumed by the keymap still counts as local keyboard activity.
    board_note_activity();
    if (record->event.pressed) {
        sleep_note_keypress();
    }
    if (!process_record_user(keycode, record)) {
        return false;
    }
    switch (keycode) {
        case RF_DFU:
            if (record->event.pressed) {
                if (device_state.link_mode != LINK_USB) return false;
                uart_send_cmd(CMD_RF_DFU, 10, 20);
            }
            return false;

        case LNK_USB:
            if (record->event.pressed) {
                board_release_keys();
            } else {
                device_state.link_mode = LINK_USB;
                uart_send_cmd(CMD_SET_LINK, 10, 10);
            }
            return false;

        case LNK_RF:
            process_channel_key(LINK_RF_24, record->event.pressed);
            return false;

        case LNK_BLE1:
            process_channel_key(LINK_BT_1, record->event.pressed);
            return false;

        case LNK_BLE2:
            process_channel_key(LINK_BT_2, record->event.pressed);
            return false;

        case LNK_BLE3:
            process_channel_key(LINK_BT_3, record->event.pressed);
            return false;

        case MAC_SEARCH:
            if (record->event.pressed) {
                register_code(KC_LGUI);
                register_code(KC_SPACE);
                wait_ms(20);
                unregister_code(KC_LGUI);
                unregister_code(KC_SPACE);
            }
            return false;

        case MAC_VOICE:
            if (record->event.pressed) {
                host_consumer_send(0xcf);
            } else {
                host_consumer_send(0);
            }
            return false;

        case MAC_DND:
            if (record->event.pressed) {
                host_system_send(0x9b);
            } else {
                host_system_send(0);
            }
            return false;

        case MAC_PRT:
            if (record->event.pressed) {
                register_code(KC_LGUI);
                register_code(KC_LSFT);
                register_code(KC_3);
            } else {
                unregister_code(KC_3);
                unregister_code(KC_LSFT);
                unregister_code(KC_LGUI);
            }
            return false;

        case MAC_PRTA:
            if (record->event.pressed) {
                register_code(KC_LGUI);
                register_code(KC_LSFT);
                register_code(KC_4);
            } else {
                unregister_code(KC_4);
                unregister_code(KC_LSFT);
                unregister_code(KC_LGUI);
            }
            return false;

        case SIDE_VAI:
            if (record->event.pressed) {
                if (side_is_low_battery() && (side_get_brightness() == 1)) return false;
                side_adjust_brightness(1);
            }
            return false;

        case SIDE_VAD:
            if (record->event.pressed) {
                side_adjust_brightness(0);
            }
            return false;

        case SIDE_MOD_A:
            if (record->event.pressed) {
                side_cycle_effect(1);
            }
            return false;

        case SIDE_MOD_B:
            if (record->event.pressed) {
                side_cycle_regions(1);
            }
            return false;

        case SIDE_HUI:
            if (record->event.pressed) {
                side_cycle_colour(1);
            }
            return false;

        case SIDE_HUD:
            if (record->event.pressed) {
                side_cycle_colour(0);
            }
            return false;

        case SIDE_SPI:
            if (record->event.pressed) {
                side_adjust_speed(1);
            }
            return false;

        case SIDE_SPD:
            if (record->event.pressed) {
                side_adjust_speed(0);
            }
            return false;

        case DEV_RESET:
            if (record->event.pressed) {
                held_key_start(&board.keys.reset, DEV_RESET, timer_read32());
                board_release_keys();
            } else if (held_key_release(&board.keys.reset, DEV_RESET, timer_read32(), BOARD_LONG_PRESS_MS) == HELD_KEY_LONG) {
                board_factory_reset();
            }
            return false;

        case SLEEP_MODE:
            if (record->event.pressed) {
                board_settings_set_sleep_enabled(!board_settings_sleep_enabled());
                side_notify_sleep_change();
            }
            return false;

        case BAT_SHOW:
            if (record->event.pressed) {
                board.rgb.mode_before_battery = rgb_matrix_get_mode();
                rgb_matrix_mode_noeeprom(RGB_MATRIX_CUSTOM_battery_link_status);
            } else {
                rgb_matrix_mode_noeeprom(board.rgb.mode_before_battery);
            }
            return false;

        case RM_VALU:
            if (side_is_low_battery() && (rgb_matrix_config.hsv.v == RGB_MATRIX_VAL_STEP)) return false;
            return true;

        case RM_TOGG:
            if (record->event.pressed) {
                rgb_matrix_enable();
                if (rgb_matrix_config.hsv.v) {
                    board.rgb.saved_brightness = rgb_matrix_config.hsv.v;
                    rgb_matrix_config.hsv.v    = 0;
                } else {
                    if (board.rgb.saved_brightness)
                        rgb_matrix_config.hsv.v = board.rgb.saved_brightness;
                    else
                        rgb_matrix_config.hsv.v = ~(RGB_MATRIX_SPD_STEP << 1);
                }
            }
            return false;

        default:
            return true;
    }
    return true;
}

/**
    @brief  timer process.
 */
static void board_tick(void) {
    if (board.first_tick) {
        board.first_tick      = false;
        board.tick_timer      = timer_read32();
        board.host.usb_driver = host_get_driver();
        // ChibiOS installs its USB driver after keyboard_post_init_kb returns.
        // Save it now, then restore the transport chosen by the physical switch.
        board_set_host_mode(device_state.link_mode == LINK_USB ? HOST_USB_TYPE : HOST_RF_TYPE);
    }

    if (timer_elapsed32(board.tick_timer) < 10) {
        return;
    } else if (timer_elapsed32(board.tick_timer) > 20) {
        board.tick_timer = timer_read32();
    } else {
        board.tick_timer += 10;
    }

    side_tick();
    if (board.idle_ticks < UINT16_MAX) {
        board.idle_ticks++;
    }
    rf_tick();
}

/**
 * @brief  Load persistent settings without changing the EEPROM schema.
 */
static void board_load_settings(void) {
    if (!board_settings_load()) {
        rgb_matrix_sethsv(RGB_DEFAULT_COLOUR, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS - RGB_MATRIX_VAL_STEP * 2);
        board_settings_reset();
    }
    side_load_settings();
}

/**
   qmk keyboard post init
 */
void keyboard_post_init_kb(void) {
    board_init_pins();
    rf_uart_init();
    wait_ms(500);
    rf_device_init();

    board_release_keys();
    board_load_settings();
    board_init_switches();
    keyboard_post_init_user();

    side_restart_link_indicator();
}

/**
   Board indicators compose with keymap indicators.
 */
bool rgb_matrix_indicators_advanced_kb(uint8_t led_min, uint8_t led_max) {
    if (!rgb_matrix_indicators_advanced_user(led_min, led_max)) return false;
    if (keymap_config.no_gui && led_min <= 72 && 72 < led_max) {
        rgb_matrix_set_color(72, 0x00, 0x80, 0x00);
    }
    return true;
}

/**
   housekeeping_task_kb
 */
void housekeeping_task_kb(void) {
    board_tick();

    rf_receive_task();

    rf_reports_task();

    rf_link_task();

    board_scan_held_keys();

    board_scan_switches();

    side_task();

    sleep_task();
}
