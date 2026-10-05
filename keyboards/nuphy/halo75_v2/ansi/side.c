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
#include "quantum.h"
#include "board_runtime.h"
#include "board_settings.h"
#include "side.h"
#include "side_tables.h"
#include "sleep.h"
//------------------------------------------------
#define SIDE_WAVE 0
#define SIDE_NEW 1
#define SIDE_CYCLE 2
#define SIDE_BREATH 3
#define SIDE_STATIC 4

#define SIDE_MODE_1 0
#define SIDE_MODE_2 1
#define SIDE_MODE_3 2
#define SIDE_MODE_4 3
#define SIDE_MODE_5 4
#define SIDE_MODE_6 5
#define SIDE_MODE_7 6
// Original EEPROM default; unlike the seven selectable masks, every region is on.
#define SIDE_MODE_ALL 7

#define LIGHT_COLOUR_MAX 8
#define SIDE_COLOUR_MAX 8
#define LIGHT_SPEED_MAX 4

// clang-format off
static const uint8_t side_speed_table[5][5] = {
    [SIDE_WAVE]   = {10, 20,  25,  30,  45},
    [SIDE_NEW]    = {30, 50,  60,  70, 100},
    [SIDE_CYCLE]  = {40, 70, 110, 180, 250},
    [SIDE_BREATH] = {40, 70, 110, 180, 250},
    [SIDE_STATIC] = {10, 20,  25,  30,  45},
};
// clang-format on

#define SIDE_BLINK_LIGHT 128
static const uint8_t side_light_table[5] = {
    0, 64, 128, 192, 255,
};

#define SIDE_INDEX 83
#define SIDE_LED_COUNT 45

static const uint8_t side_led_index_tab[SIDE_LED_COUNT] = {
    SIDE_INDEX + 10, SIDE_INDEX + 11, SIDE_INDEX + 12, SIDE_INDEX + 13, SIDE_INDEX + 14, SIDE_INDEX + 15, SIDE_INDEX + 16, SIDE_INDEX + 17, SIDE_INDEX + 18, SIDE_INDEX + 19, SIDE_INDEX + 20, SIDE_INDEX + 21, SIDE_INDEX + 22, SIDE_INDEX + 23, SIDE_INDEX + 24, SIDE_INDEX + 25, SIDE_INDEX + 26, SIDE_INDEX + 27,

    SIDE_INDEX + 0,  SIDE_INDEX + 1,  SIDE_INDEX + 2,  SIDE_INDEX + 3,  SIDE_INDEX + 4,

    SIDE_INDEX + 28, SIDE_INDEX + 29, SIDE_INDEX + 30, SIDE_INDEX + 31, SIDE_INDEX + 32, SIDE_INDEX + 33, SIDE_INDEX + 34, SIDE_INDEX + 35, SIDE_INDEX + 36, SIDE_INDEX + 37, SIDE_INDEX + 38, SIDE_INDEX + 39, SIDE_INDEX + 40, SIDE_INDEX + 41, SIDE_INDEX + 42, SIDE_INDEX + 43, SIDE_INDEX + 44,

    SIDE_INDEX + 9,  SIDE_INDEX + 8,  SIDE_INDEX + 7,  SIDE_INDEX + 6,  SIDE_INDEX + 5,
};

typedef struct {
    bool     pending;
    bool     active;
    uint32_t timer;
} side_notification_t;

typedef struct {
    board_side_settings_t settings;
    struct {
        uint8_t  led_count;
        uint8_t  region_mask;
        uint8_t  point;
        uint8_t  previous_colour;
        uint8_t  breathe_point;
        uint8_t  breathe_colour;
        uint16_t elapsed;
        uint32_t timer;
    } effect;
    struct {
        bool    awaiting_dial;
        bool    active;
        uint8_t index;
        uint8_t pwm[SIDE_LED_COUNT];
    } startup;
    struct {
        bool     initial;
        bool     charging;
        bool     low;
        uint8_t  charge_state;
        uint8_t  percent;
        uint8_t  blinks;
        uint8_t  breathe_point;
        uint32_t show_timer;
        uint32_t status_debounce;
        uint32_t percent_debounce;
        uint32_t blink_timer;
        uint32_t breathe_timer;
#if CHARGING_SHIFT
        uint16_t shift_mask;
        bool     shift_reversing;
        uint32_t shift_timer;
#endif
    } battery;
    struct {
        bool     initial;
        uint8_t  blinks;
        uint16_t show_ticks;
        uint32_t blink_timer;
    } link;
    side_notification_t system;
    side_notification_t sleep;
    struct {
        uint8_t r;
        uint8_t g;
        uint8_t b;
    } colour;
} side_context_t;

static side_context_t side = {
    .settings = {.effect = SIDE_CYCLE, .regions = SIDE_MODE_ALL, .brightness = 2, .speed = 2, .rainbow = 1},
    .effect   = {.led_count = SIDE_LED_COUNT, .region_mask = 0x1f},
    .startup  = {.awaiting_dial = true, .active = true},
    .battery  = {.initial = true, .charging = true, .blinks = 6},
    .link     = {.initial = true},
};

void side_load_settings(void) {
    // The EEPROM signature validates the schema, not the individual indices.
    // Normalize once at the boundary before any animation accesses a table.
    const board_side_settings_t saved = board_settings_get_side();
    side.settings.effect              = saved.effect <= SIDE_STATIC ? saved.effect : SIDE_CYCLE;
    side.settings.regions             = saved.regions <= SIDE_MODE_ALL ? saved.regions : SIDE_MODE_ALL;
    side.settings.brightness          = saved.brightness <= 4 ? saved.brightness : 2;
    side.settings.speed               = saved.speed <= LIGHT_SPEED_MAX ? saved.speed : 2;
    side.settings.rainbow             = saved.rainbow != 0;

    const uint8_t colour_count  = side.settings.effect == SIDE_NEW ? 3 : LIGHT_COLOUR_MAX;
    side.settings.colour        = saved.colour < colour_count ? saved.colour : 0;
    side.effect.point           = 0;
    // Keep the next settings write coherent without adding a write on boot.
    board_settings_set_side(&side.settings);
}

// Update only the preferences changed by the current control. Battery limiting
// may have temporarily lowered runtime brightness and must not save that clamp.
static void side_save_settings(const board_side_settings_t *settings) {
    board_settings_set_side(settings);
    board_settings_save();
}

void side_tick(void) {
    if (side.link.show_ticks < RF_LINK_SHOW_TIME) {
        side.link.show_ticks++;
    }
}

void side_set_link_blinks(uint8_t count) {
    side.link.blinks = count;
}

void side_restart_link_indicator(void) {
    side.link.show_ticks = 0;
}

void side_notify_system_change(void) {
    side.system.pending = true;
}

void side_notify_sleep_change(void) {
    side.sleep.pending = true;
}

bool side_is_low_battery(void) {
    return side.battery.low;
}

uint8_t side_get_brightness(void) {
    return side.settings.brightness;
}

/**
 * @brief  Adjusting the brightness of side lights.
 * @param  dir: 0 - decrease, 1 - increase.
 * @note  save to eeprom.
 */
void side_adjust_brightness(uint8_t brighten) {
    if (brighten) {
        if (side.settings.brightness == 4) return;
        side.settings.brightness++;
    } else {
        if (side.settings.brightness == 0) return;
        side.settings.brightness--;
    }
    board_side_settings_t saved = board_settings_get_side();
    saved.brightness            = side.settings.brightness;
    side_save_settings(&saved);
}

/**
 * @brief  Adjusting the speed of side lights.
 * @param  dir: 0 - decrease, 1 - increase.
 * @note  save to eeprom.
 */
void side_adjust_speed(uint8_t fast) {
    if (side.settings.speed > LIGHT_SPEED_MAX) side.settings.speed = LIGHT_SPEED_MAX / 2;

    if (fast) {
        if (side.settings.speed) side.settings.speed--;
    } else {
        if (side.settings.speed < LIGHT_SPEED_MAX) side.settings.speed++;
    }
    board_side_settings_t saved = board_settings_get_side();
    saved.speed                 = side.settings.speed;
    side_save_settings(&saved);
}

/**
 * @brief  Switch to the next color of side lights.
 * @param  dir: 0 - prev, 1 - next.
 * @note  save to eeprom.
 */
void side_cycle_colour(uint8_t dir) {
    const uint8_t colour_count = side.settings.effect == SIDE_NEW ? 3 : 8;
    if ((side.settings.effect != SIDE_WAVE) && (side.settings.effect != SIDE_BREATH)) {
        if (side.settings.rainbow) {
            side.settings.rainbow = 0;
            side.settings.colour  = 0;
        }
    }

    if (dir) {
        if (side.settings.rainbow) {
            side.settings.rainbow = 0;
            side.settings.colour  = 0;
        } else {
            side.settings.colour++;
            if (side.settings.colour >= colour_count) {
                side.settings.rainbow = 1;
                side.settings.colour  = 0;
            }
        }
    } else {
        if (side.settings.rainbow) {
            side.settings.rainbow = 0;
            side.settings.colour  = colour_count - 1;
        } else {
            side.settings.colour--;
            if (side.settings.colour >= colour_count) {
                side.settings.rainbow = 1;
                side.settings.colour  = 0;
            }
        }
    }
    // Rainbow and single-colour waves use different lookup-table lengths.
    side.effect.point           = 0;
    board_side_settings_t saved = board_settings_get_side();
    saved.rainbow               = side.settings.rainbow;
    saved.colour                = side.settings.colour;
    side_save_settings(&saved);
}

/**
 * @brief  Change the color mode of side lights.
 * @param  dir: 0 - prev, 1 - next.
 * @note  save to eeprom.
 */
void side_cycle_effect(uint8_t dir) {
    if (dir) {
        side.settings.effect++;
        if (side.settings.effect > SIDE_STATIC) {
            side.settings.effect = 0;
        }
    } else {
        if (side.settings.effect > 0) {
            side.settings.effect--;
        } else {
            side.settings.effect = SIDE_STATIC;
        }
    }
    if (side.settings.effect == SIDE_NEW) {
        side.effect.previous_colour = side.settings.colour;
        side.settings.colour        = 0;
    } else if (side.settings.effect == SIDE_BREATH) {
        side.settings.colour = side.effect.previous_colour;
    }

    side.effect.point           = 0;
    board_side_settings_t saved = board_settings_get_side();
    saved.effect                = side.settings.effect;
    // Entering the dual-colour palette also changes its index; persist both.
    saved.colour = side.settings.colour;
    side_save_settings(&saved);
}

void side_cycle_regions(uint8_t dir) {
    if (dir) {
        side.settings.regions++;
        if (side.settings.regions > SIDE_MODE_7) {
            side.settings.regions = SIDE_MODE_1;
        }
    } else {
        if (side.settings.regions > 0) {
            side.settings.regions--;
        } else {
            side.settings.regions = SIDE_MODE_1;
        }
    }
    side.effect.point           = 0;
    board_side_settings_t saved = board_settings_get_side();
    saved.regions               = side.settings.regions;
    side_save_settings(&saved);
}

/**
 * @brief  set left side leds.
 * @param  ...
 */
static void set_left_rgb(uint8_t r, uint8_t g, uint8_t b) {
    for (int i = 0; i < 5; i++)
        rgb_matrix_set_color(SIDE_INDEX + i, r, g, b);
}

static void set_all_side_off(void) {
    for (int i = 0; i < SIDE_LED_COUNT; i++)
        rgb_matrix_set_color(SIDE_INDEX + i, 0, 0, 0);
}

/**
 * @brief  mac or win system indicate
 */
static void sys_sw_led_show(void) {
    if (side.system.pending) {
        side.system.pending = false;
        side.system.timer   = timer_read32(); // store time of last refresh
        side.system.active  = true;
    }

    if (side.system.active) {
        if (device_state.system_mode == SYS_SW_MAC) {
            side.colour.r = colour_lib[7][0];
            side.colour.g = colour_lib[7][1];
            side.colour.b = colour_lib[7][2];
        } else {
            side.colour.r = colour_lib[5][0];
            side.colour.g = colour_lib[5][1];
            side.colour.b = colour_lib[5][2];
        }
        if ((timer_elapsed32(side.system.timer) / 500) % 2 == 0) {
            set_left_rgb(side.colour.r, side.colour.g, side.colour.b);
        } else {
            set_left_rgb(0x00, 0x00, 0x00);
        }
        if (timer_elapsed32(side.system.timer) >= (3000 - 50)) {
            side.system.active = false;
        }
    }
}

/**
 * @brief  sleep enable or disable indicate
 */
static void sleep_sw_led_show(void) {
    if (side.sleep.pending) {
        side.sleep.pending = false;
        side.sleep.timer   = timer_read32(); // store time of last refresh
        side.sleep.active  = true;
    }

    if (side.sleep.active) {
        if (board_settings_sleep_enabled()) {
            side.colour.r = 0x00;
            side.colour.g = SIDE_BLINK_LIGHT;
            side.colour.b = 0x00;
        } else {
            side.colour.r = 0xff;
            side.colour.g = 0x00;
            side.colour.b = 0x00;
        }
        if ((timer_elapsed32(side.sleep.timer) / 500) % 2 == 0) {
            set_left_rgb(side.colour.r, side.colour.g, side.colour.b);
        } else {
            set_left_rgb(0x00, 0x00, 0x00);
        }
        if (timer_elapsed32(side.sleep.timer) >= (3000 - 50)) {
            side.sleep.active = false;
        }
    }
}

/**
 * @brief  host system led indicate.
 */
static void sys_led_show(void) {
    if (device_state.link_mode == LINK_USB) {
        // caps lock led
        if (host_keyboard_led_state().caps_lock) {
            set_left_rgb(colour_lib[4][0], colour_lib[4][1], colour_lib[4][2]);
        }
    } else {
        if (device_state.keyboard_leds & 0x02) {
            set_left_rgb(colour_lib[4][0], colour_lib[4][1], colour_lib[4][2]);
        }
    }
}

/**
 * @brief  light_point_playing.
 * @param trend:
 * @param step:
 * @param len:
 * @param point:
 */
static void light_point_playing(uint8_t trend, uint8_t step, uint8_t len, uint8_t *point) {
    if (trend) {
        *point += step;
        if (*point >= len) *point -= len;
    } else {
        *point -= step;
        if (*point >= len) *point = len - (255 - *point) - 1;
    }
}

/**
 * @brief  count_rgb_light.
 * @param light_temp:
 */
static void count_rgb_light(uint8_t light_temp) {
    uint16_t temp;

    temp          = (light_temp)*side.colour.r + side.colour.r;
    side.colour.r = temp >> 8;

    temp          = (light_temp)*side.colour.g + side.colour.g;
    side.colour.g = temp >> 8;

    temp          = (light_temp)*side.colour.b + side.colour.b;
    side.colour.b = temp >> 8;
}

/**
 * @brief  auxiliary_rgb_light.
 */
static bool is_side_rgb_on(uint8_t index) {
    if ((index <= 10) || ((index >= 37) && (index <= 39))) return side.effect.region_mask & 0x01;
    if ((((index >= 11) && (index <= 17)) || ((index >= 23) && (index <= 29)) || ((index >= 32) && (index <= 36)))) return side.effect.region_mask & 0x02;
    if (((index >= 40) && (index <= 44))) return side.effect.region_mask & 0x04;
    if (((index >= 18) && (index <= 22))) return side.effect.region_mask & 0x08;
    if (((index >= 30) && (index <= 31))) return side.effect.region_mask & 0x10;
    return false;
}

static void side_power_mode_show(void) {
    if (side.effect.elapsed <= side_speed_table[0][side.settings.speed])
        return;
    else
        side.effect.elapsed -= side_speed_table[0][side.settings.speed];
    if (side.effect.elapsed > 20) side.effect.elapsed = 0;

    // A completed sweep only fades existing LEDs; index 45 is outside the buffer.
    if (side.startup.index < SIDE_LED_COUNT) {
        side.startup.pwm[side.startup.index] = 0xff;
        side.startup.index++;
    }

    uint8_t i;

    for (i = 0; i < SIDE_LED_COUNT; i++) {
        side.colour.r = colour_lib[side.settings.colour][0];
        side.colour.g = colour_lib[side.settings.colour][1];
        side.colour.b = colour_lib[side.settings.colour][2];

        count_rgb_light(side.startup.pwm[i]);
        count_rgb_light(side_light_table[2]);
        rgb_matrix_set_color(side_led_index_tab[i], side.colour.r, side.colour.g, side.colour.b);
    }

    for (i = 0; i < SIDE_LED_COUNT; i++) {
        if (side.startup.pwm[i] & 0x80)
            side.startup.pwm[i] -= 8;
        else if (side.startup.pwm[i] & 0x40)
            side.startup.pwm[i] -= 6;
        else if (side.startup.pwm[i] & 0x20)
            side.startup.pwm[i] -= 4;
        else if (side.startup.pwm[i] & 0x10)
            side.startup.pwm[i] -= 3;
        else if (side.startup.pwm[i] & 0x08)
            side.startup.pwm[i] -= 2;
        else if (side.startup.pwm[i])
            side.startup.pwm[i]--;
    }

    if (side.startup.pwm[44] == 1) {
        side.startup.active   = 0;
        side.link.show_ticks  = 0;
        side.battery.charging = true;
    }
}

/**
 * @brief  side_wave_mode_show.
 */
static void side_wave_mode_show(void) {
    uint8_t play_index;

    if (side.effect.elapsed <= side_speed_table[side.settings.effect][side.settings.speed]) return;
    side.effect.elapsed -= side_speed_table[side.settings.effect][side.settings.speed];
    if (side.effect.elapsed > 20) side.effect.elapsed = 0;

    if (side.settings.rainbow)
        light_point_playing(0, 1, FLOW_COLOUR_TAB_LEN, &side.effect.point);
    else
        light_point_playing(0, 1, WAVE_TAB_LEN, &side.effect.point);

    if (side.effect.led_count == 0) {
        set_all_side_off();
        return;
    }
    play_index = side.effect.point;
    for (int i = 0; i < side.effect.led_count; i++) {
        if (side.settings.rainbow) {
            side.colour.r = flow_rainbow_colour_tab[play_index][0];
            side.colour.g = flow_rainbow_colour_tab[play_index][1];
            side.colour.b = flow_rainbow_colour_tab[play_index][2];
            light_point_playing(1, 5, FLOW_COLOUR_TAB_LEN, &play_index);
        } else {
            side.colour.r = colour_lib[side.settings.colour][0];
            side.colour.g = colour_lib[side.settings.colour][1];
            side.colour.b = colour_lib[side.settings.colour][2];
            light_point_playing(1, 5, WAVE_TAB_LEN, &play_index);
            count_rgb_light(wave_data_tab[play_index]);
        }

        count_rgb_light(side_light_table[side.settings.brightness]);
        if (is_side_rgb_on(i))
            rgb_matrix_set_color(side_led_index_tab[i], side.colour.r, side.colour.g, side.colour.b);
        else
            rgb_matrix_set_color(side_led_index_tab[i], 0, 0, 0);
    }
}

static void side_new_mode_show(void) {
    uint8_t play_index;

    if (side.effect.elapsed <= side_speed_table[side.settings.effect][side.settings.speed]) return;
    side.effect.elapsed -= side_speed_table[side.settings.effect][side.settings.speed];
    if (side.effect.elapsed > 20) side.effect.elapsed = 0;

    if (side.effect.led_count == 0) {
        set_all_side_off();
        return;
    }
    light_point_playing(0, 1, side.effect.led_count, &side.effect.point);
    play_index = side.effect.point;
    for (int i = 0; i < side.effect.led_count; i++) {
        if (play_index * 2 < side.effect.led_count) {
            side.colour.r = dual_colour_lib[side.settings.colour][0];
            side.colour.g = dual_colour_lib[side.settings.colour][1];
            side.colour.b = dual_colour_lib[side.settings.colour][2];
        } else {
            side.colour.r = dual_colour_lib[side.settings.colour][3];
            side.colour.g = dual_colour_lib[side.settings.colour][4];
            side.colour.b = dual_colour_lib[side.settings.colour][5];
        }
        light_point_playing(1, 1, side.effect.led_count, &play_index);
        count_rgb_light(side_light_table[side.settings.brightness]);
        if (is_side_rgb_on(i))
            rgb_matrix_set_color(side_led_index_tab[i], side.colour.r, side.colour.g, side.colour.b);
        else
            rgb_matrix_set_color(side_led_index_tab[i], 0, 0, 0);
    }
}

static void side_cycle_mode_show(void) {
    if (side.effect.elapsed <= side_speed_table[side.settings.effect][side.settings.speed]) return;
    side.effect.elapsed -= side_speed_table[side.settings.effect][side.settings.speed];
    if (side.effect.elapsed > 20) side.effect.elapsed = 0;

    light_point_playing(1, 1, FLOW_COLOUR_TAB_LEN, &side.effect.point);
    if (side.effect.led_count == 0) {
        set_all_side_off();
        return;
    }
    side.colour.r = flow_rainbow_colour_tab[side.effect.point][0];
    side.colour.g = flow_rainbow_colour_tab[side.effect.point][1];
    side.colour.b = flow_rainbow_colour_tab[side.effect.point][2];
    count_rgb_light(side_light_table[side.settings.brightness]);
    for (int i = 0; i < side.effect.led_count; i++) {
        if (is_side_rgb_on(i))
            rgb_matrix_set_color(side_led_index_tab[i], side.colour.r, side.colour.g, side.colour.b);
        else
            rgb_matrix_set_color(side_led_index_tab[i], 0, 0, 0);
    }
}

static void side_breathe_mode_show(void) {
    if (side.effect.elapsed <= side_speed_table[side.settings.effect][side.settings.speed])
        return;
    else
        side.effect.elapsed -= side_speed_table[side.settings.effect][side.settings.speed];
    if (side.effect.elapsed > 20) side.effect.elapsed = 0;

    if (side.effect.led_count == 0) {
        set_all_side_off();
        return;
    }

    light_point_playing(0, 1, BREATHE_TAB_LEN, &side.effect.breathe_point);
    if (side.settings.rainbow) {
        if (side.effect.breathe_point == 0 && ++side.effect.breathe_colour >= LIGHT_COLOUR_MAX) side.effect.breathe_colour = 0;
        side.colour.r = colour_lib[side.effect.breathe_colour][0];
        side.colour.g = colour_lib[side.effect.breathe_colour][1];
        side.colour.b = colour_lib[side.effect.breathe_colour][2];

    } else {
        side.colour.r = colour_lib[side.settings.colour][0];
        side.colour.g = colour_lib[side.settings.colour][1];
        side.colour.b = colour_lib[side.settings.colour][2];
    }
    count_rgb_light(breathe_data_tab[side.effect.breathe_point]);
    count_rgb_light(side_light_table[side.settings.brightness]);

    for (int i = 0; i < side.effect.led_count; i++) {
        if (is_side_rgb_on(i))
            rgb_matrix_set_color(side_led_index_tab[i], side.colour.r, side.colour.g, side.colour.b);
        else
            rgb_matrix_set_color(side_led_index_tab[i], 0, 0, 0);
    }
}

/**
 * @brief  side_static_mode_show.
 */
static void side_static_mode_show(void) {
    if (side.effect.elapsed <= side_speed_table[side.settings.effect][side.settings.speed])
        return;
    else
        side.effect.elapsed -= side_speed_table[side.settings.effect][side.settings.speed];
    if (side.effect.elapsed > 20) side.effect.elapsed = 0;

    if (side.effect.led_count == 0) set_all_side_off();

    if (side.effect.point >= SIDE_COLOUR_MAX) side.effect.point = 0;

    for (int i = 0; i < side.effect.led_count; i++) {
        side.colour.r = colour_lib[side.settings.colour][0];
        side.colour.g = colour_lib[side.settings.colour][1];
        side.colour.b = colour_lib[side.settings.colour][2];
        count_rgb_light(side_light_table[side.settings.brightness]);
        if (is_side_rgb_on(i))
            rgb_matrix_set_color(side_led_index_tab[i], side.colour.r, side.colour.g, side.colour.b);
        else
            rgb_matrix_set_color(side_led_index_tab[i], 0, 0, 0);
    }
}

/**
 * @brief  bat_chargeing_breathe.
 */
#if !CHARGING_SHIFT
static void bat_charging_breathe(void) {
    if (timer_elapsed32(side.battery.breathe_timer) > 30) {
        side.battery.breathe_timer = timer_read32();
        light_point_playing(0, 2, BREATHE_TAB_LEN, &side.battery.breathe_point);
    }

    side.colour.r = 0x80;
    side.colour.g = 0x40;
    side.colour.b = 0x00;
    count_rgb_light(breathe_data_tab[side.battery.breathe_point]);
    set_left_rgb(side.colour.r, side.colour.g, side.colour.b);
}

/**
 * @brief  bat_chargeing_design.
 */
#else
static void bat_charging_design(uint8_t init, uint8_t r, uint8_t g, uint8_t b) {
    uint16_t bit_mask = 1;
    uint8_t  i;

    if (timer_elapsed32(side.battery.shift_timer) > 100) {
        side.battery.shift_timer = timer_read32();

        if (side.battery.shift_reversing) {
            side.battery.shift_mask >>= 1;
            if (side.battery.shift_mask == 0x1f >> (side.effect.led_count - init)) side.battery.shift_reversing = 0;
        } else {
            side.battery.shift_mask <<= 1;
            side.battery.shift_mask |= 1;
            if (side.battery.shift_mask == 0x7f) side.battery.shift_reversing = 1;
        }
    }

    for (i = 0; i < side.effect.led_count; i++) {
        if (side.battery.shift_mask & bit_mask) {
            rgb_matrix_set_color(i, r, g, b);
        } else {
            rgb_matrix_set_color(i, 0x00, 0x00, 0x00);
        }
        bit_mask <<= 1;
    }
}

/**
 * @brief  rf state indicate
 */
#endif

#define RF_LED_LINK_PERIOD 500
#define RF_LED_PAIR_PERIOD 250
static void rf_led_show(void) {
    uint16_t blink_period = 0;

    if (device_state.link_mode == LINK_RF_24) {
        side.colour.r = colour_lib[3][0];
        side.colour.g = colour_lib[3][1];
        side.colour.b = colour_lib[3][2];
    } else if (device_state.link_mode == LINK_USB) {
        side.colour.r = colour_lib[2][0];
        side.colour.g = colour_lib[2][1];
        side.colour.b = colour_lib[2][2];
        if (side.link.initial && (side.link.show_ticks < RF_LINK_SHOW_TIME)) return;
    } else {
        side.colour.r = colour_lib[5][0];
        side.colour.g = colour_lib[5][1];
        side.colour.b = colour_lib[5][2];
    }

    side.link.initial = 0;

    if (side.link.blinks) {
        if (device_state.rf_state == RF_PAIRING)
            blink_period = RF_LED_PAIR_PERIOD;
        else
            blink_period = RF_LED_LINK_PERIOD;

        if (timer_elapsed32(side.link.blink_timer) < (blink_period >> 1)) {
        } else {
            side.colour.r = 0x00;
            side.colour.g = 0x00;
            side.colour.b = 0x00;
        }

        if (timer_elapsed32(side.link.blink_timer) >= blink_period) {
            side.link.blinks--;
            side.link.blink_timer = timer_read32();
        }
    } else if (side.link.show_ticks < RF_LINK_SHOW_TIME) {
    } else {
        side.link.blink_timer = timer_read32();
        return;
    }

    set_left_rgb(side.colour.r, side.colour.g, side.colour.b);
}

#define LOW_BAT_BLINK_PRIOD 500
static void low_bat_show(void) {
    side.colour.r = 0x80, side.colour.g = 0, side.colour.b = 0;

    if (side.battery.blinks) {
        if (timer_elapsed32(side.battery.blink_timer) > (LOW_BAT_BLINK_PRIOD >> 1)) {
            side.colour.r = 0x00;
            side.colour.g = 0x00;
            side.colour.b = 0x00;
        }

        if (timer_elapsed32(side.battery.blink_timer) >= LOW_BAT_BLINK_PRIOD) {
            side.battery.blink_timer = timer_read32();
            side.battery.blinks--;
        }
    }
    set_left_rgb(side.colour.r, side.colour.g, side.colour.b);
}

/**
 * @brief  Battery level indicator
 */
static void bat_percent_led(uint8_t bat_percent) {
    uint8_t i;
    uint8_t bat_end_led;
    uint8_t bat_r, bat_g, bat_b;

    if (bat_percent <= 20) {
        bat_end_led = 1;
        bat_r       = colour_lib[0][0];
        bat_g       = colour_lib[0][1];
        bat_b       = colour_lib[0][2];
    } else if (bat_percent <= 50) {
        bat_end_led = 2;
        bat_r       = colour_lib[1][0];
        bat_g       = colour_lib[1][1];
        bat_b       = colour_lib[1][2];
    } else if (bat_percent <= 80) {
        bat_end_led = 4;
        bat_r       = colour_lib[2][0];
        bat_g       = colour_lib[2][1];
        bat_b       = colour_lib[2][2];
    } else {
        bat_end_led = 5;
        bat_r       = colour_lib[3][0];
        bat_g       = colour_lib[3][1];
        bat_b       = colour_lib[3][2];
    }
    if (side.battery.charging) {
        side.battery.blinks = 6;
#if (CHARGING_SHIFT)
        bat_charging_design(bat_end_led, bat_r >> 2, bat_g >> 2, bat_b >> 2);
#else
        bat_charging_breathe();
#endif
    } else if (bat_percent < 10) {
        low_bat_show();
    } else {
        bat_end_led         = 4;
        side.battery.blinks = 6;
        for (i = 0; i <= bat_end_led; i++)
            rgb_matrix_set_color(SIDE_INDEX + i, bat_r, bat_g, bat_b);
    }
}

/**
 * @brief  battery state indicate
 */
static void bat_led_show(void) {
    if (device_state.link_mode != LINK_USB) {
        if (side.link.show_ticks < RF_LINK_SHOW_TIME) return;

        if (device_state.rf_state != RF_CONNECT) return;
    }

    if (side.battery.initial) {
        side.battery.initial      = 0;
        side.battery.show_timer   = timer_read32();
        side.battery.charge_state = device_state.charge_state;
        side.battery.percent      = device_state.battery_percent;
    }

    if (side.battery.charge_state != device_state.charge_state) {
        if (timer_elapsed32(side.battery.status_debounce) > 1000) {
            if (((side.battery.charge_state & 0x01) == 0) && ((device_state.charge_state & 0x01) != 0)) {
                side.battery.charging   = true;
                side.battery.show_timer = timer_read32();
            }
            side.battery.charge_state = device_state.charge_state;
        }
    } else {
        side.battery.status_debounce = timer_read32();
        if (side.battery.charging) {
            if (timer_elapsed32(side.battery.show_timer) > 10000) {
                side.battery.charging = false;
            }
        }
        if (side.battery.charge_state == 0x03) {
            side.battery.charging = true;
        } else if (!(side.battery.charge_state & 0x01)) {
            side.battery.charging = 0;
        }
    }

    if (side.battery.percent != device_state.battery_percent) {
        if (timer_elapsed32(side.battery.percent_debounce) > 1000) {
            side.battery.percent = device_state.battery_percent;
        }
    } else {
        side.battery.percent_debounce = timer_read32();

        if ((side.battery.percent < 10) && (!(side.battery.charge_state & 0x01))) {
            side.battery.show_timer = timer_read32();
            side.battery.low        = 1;
            if (rgb_matrix_config.hsv.v > RGB_MATRIX_VAL_STEP) {
                rgb_matrix_config.hsv.v = RGB_MATRIX_VAL_STEP;
            }

            if (side.settings.brightness > 1) {
                side.settings.brightness = 1;
            }
        } else
            side.battery.low = 0;
    }
    // The battery indicator is always enabled, as in the factory configuration.
    bat_percent_led(side.battery.percent);
}

/**
 * @brief  side_show_reset.
 */
void side_show_reset(void) {
    sleep_set_active(false);
    for (int blink_cnt = 0; blink_cnt < 3; blink_cnt++) {
        rgb_matrix_set_color_all(0xFF, 0xFF, 0xFF);
        rgb_matrix_update_pwm_buffers();
        wait_ms(200);

        rgb_matrix_set_color_all(0x00, 0x00, 0x00);
        rgb_matrix_update_pwm_buffers();
        wait_ms(200);
    }
}

void side_reset_settings(void) {
    side.effect.elapsed = 0;
    side.effect.timer   = timer_read32();

    rgb_matrix_enable();
    rgb_matrix_mode(RGB_MATRIX_DEFAULT_MODE);
    rgb_matrix_set_speed(255 - RGB_MATRIX_SPD_STEP * 2);
    rgb_matrix_sethsv(RGB_DEFAULT_COLOUR, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS - RGB_MATRIX_VAL_STEP * 2);
    board_settings_reset();
    side_load_settings();
}

/**
 * @brief  rgb test
 */
void side_show_test(void) {
    sleep_set_active(false);
    rgb_matrix_set_color_all(0xFF, 0x00, 0x00);
    rgb_matrix_update_pwm_buffers();
    wait_ms(1000);
    rgb_matrix_set_color_all(0x00, 0xFF, 0x00);
    rgb_matrix_update_pwm_buffers();
    wait_ms(1000);
    rgb_matrix_set_color_all(0x00, 0x00, 0xFF);
    rgb_matrix_update_pwm_buffers();
    wait_ms(1000);
}

/**
 * @brief  side_led_show.
 */
void side_task(void) {
    if (rgb_matrix_get_suspend_state()) {
        side.effect.timer = timer_read32();
        return;
    }

    side.effect.elapsed += timer_elapsed32(side.effect.timer);
    side.effect.timer = timer_read32();

    if (side.startup.awaiting_dial) {
        if (!board_dial_is_ready()) return;
        side.startup.awaiting_dial = 0;
    }

    if (side.startup.active) {
        side_power_mode_show();
        return;
    }

    switch (side.settings.regions) {
        case SIDE_MODE_1:
            side.effect.led_count   = 0;
            side.effect.region_mask = 0;
            break;

        case SIDE_MODE_2:
            side.effect.led_count   = SIDE_LED_COUNT;
            side.effect.region_mask = 0x10;
            break;

        case SIDE_MODE_3:
            side.effect.led_count   = SIDE_LED_COUNT;
            side.effect.region_mask = 0x11;
            break;

        case SIDE_MODE_4:
            side.effect.led_count   = SIDE_LED_COUNT;
            side.effect.region_mask = 0x15;
            break;

        case SIDE_MODE_5:
            side.effect.led_count   = SIDE_LED_COUNT;
            side.effect.region_mask = 0x12;
            break;

        case SIDE_MODE_6:
            side.effect.led_count   = SIDE_LED_COUNT;
            side.effect.region_mask = 0x16;
            break;

        case SIDE_MODE_7:
            side.effect.led_count   = SIDE_LED_COUNT;
            side.effect.region_mask = 0x17;
            break;
        case SIDE_MODE_ALL:
            side.effect.led_count   = SIDE_LED_COUNT;
            side.effect.region_mask = 0x1f;
            break;
        default:
            break;
    }

    switch (side.settings.effect) {
        case SIDE_WAVE:
            side_wave_mode_show();
            break;
        case SIDE_NEW:
            side_new_mode_show();
            break;
        case SIDE_CYCLE:
            side_cycle_mode_show();
            break;
        case SIDE_BREATH:
            side_breathe_mode_show();
            break;
        case SIDE_STATIC:
            side_static_mode_show();
            break;
    }

    bat_led_show();
    sys_led_show();
    sys_sw_led_show();
    sleep_sw_led_show();
    rf_led_show();
}
