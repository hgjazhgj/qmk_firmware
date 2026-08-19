RGB_MATRIX_EFFECT(game_mode)
RGB_MATRIX_EFFECT(position_mode)
RGB_MATRIX_EFFECT(hgjazhgj)
RGB_MATRIX_EFFECT(battery_link_status)

#ifdef RGB_MATRIX_CUSTOM_EFFECT_IMPLS

#include "ansi.h"

extern rgb_config_t rgb_matrix_config;
extern DEV_INFO_STRUCT dev_info;

#define HGJAZHGJ_KEY_LED_COUNT 83

static uint8_t      hgjazhgj_hue[HGJAZHGJ_KEY_LED_COUNT];
static uint32_t     hgjazhgj_fade_started[HGJAZHGJ_KEY_LED_COUNT];
static matrix_row_t hgjazhgj_pressed[MATRIX_ROWS];

static uint16_t hgjazhgj_fade_duration(void) {
    if (rgb_matrix_config.speed & 0x80) {
        return 1000 - ((uint16_t)(rgb_matrix_config.speed - 127) * 900 >> 7);
    }
    return 1000 + ((uint16_t)(127 - rgb_matrix_config.speed) * 2000 >> 7);
}

static bool hgjazhgj(effect_params_t* params) {
    if (params->iter == 0) {
        if (params->init) {
            for (uint8_t i = 0; i < HGJAZHGJ_KEY_LED_COUNT; i++) {
                hgjazhgj_hue[i]          = random8();
                hgjazhgj_fade_started[i] = g_rgb_timer - hgjazhgj_fade_duration();
            }
            for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
                hgjazhgj_pressed[row] = matrix_get_row(row);
            }
        } else {
            for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
                matrix_row_t pressed = matrix_get_row(row);
                matrix_row_t new_keys = pressed & ~hgjazhgj_pressed[row];

                for (uint8_t col = 0; col < MATRIX_COLS; col++) {
                    if (new_keys & ((matrix_row_t)1 << col)) {
                        uint8_t led = g_led_config.matrix_co[row][col];
                        if (led < HGJAZHGJ_KEY_LED_COUNT) {
                            hgjazhgj_hue[led]          = random8();
                            hgjazhgj_fade_started[led] = g_rgb_timer;
                        }
                    }
                }
                hgjazhgj_pressed[row] = pressed;
            }
        }
    }

    RGB_MATRIX_USE_LIMITS(led_min, led_max);
    uint16_t duration = hgjazhgj_fade_duration();

    for (uint8_t i = led_min; i < led_max && i < HGJAZHGJ_KEY_LED_COUNT; i++) {
        uint32_t elapsed = g_rgb_timer - hgjazhgj_fade_started[i];
        uint8_t  value   = elapsed >= duration ? rgb_matrix_config.hsv.v :
                           ((uint32_t)rgb_matrix_config.hsv.v * elapsed / duration);
        rgb_t rgb = rgb_matrix_hsv_to_rgb((hsv_t){hgjazhgj_hue[i], 255, value});
        rgb_matrix_set_color(i, rgb.r, rgb.g, rgb.b);
    }

    return rgb_matrix_check_finished_leds(led_max);
}

static bool game_mode(effect_params_t* params) {
    RGB_MATRIX_USE_LIMITS(led_min, led_max);

    for (uint8_t i = led_min; i < led_max; i++) {
        if(i<83)
        rgb_matrix_set_color(i, 0x00, 0x00, 0x00);
    }

    RGB rgb = hsv_to_rgb(rgb_matrix_config.hsv);
    rgb_matrix_set_color(0, rgb.r, rgb.g, rgb.b);       // ESC

    rgb_matrix_set_color(33, rgb.r, rgb.g, rgb.b);      // W
    rgb_matrix_set_color(47, rgb.r, rgb.g, rgb.b);      // D
    rgb_matrix_set_color(48, rgb.r, rgb.g, rgb.b);      // S
    rgb_matrix_set_color(49, rgb.r, rgb.g, rgb.b);      // A

    rgb_matrix_set_color(72, rgb.r, rgb.g, rgb.b);      // up
    rgb_matrix_set_color(80, rgb.r, rgb.g, rgb.b);      // right
    rgb_matrix_set_color(81, rgb.r, rgb.g, rgb.b);      // down
    rgb_matrix_set_color(82, rgb.r, rgb.g, rgb.b);      // left

    return rgb_matrix_check_finished_leds(led_max);
}

static bool position_mode(effect_params_t* params) {
    RGB_MATRIX_USE_LIMITS(led_min, led_max);

    for (uint8_t i = led_min; i < led_max; i++) {
        if(i<83)
        rgb_matrix_set_color(i, 0x00, 0x00, 0x00);
    }

    RGB rgb = hsv_to_rgb(rgb_matrix_config.hsv);

    rgb_matrix_set_color(50, rgb.r, rgb.g, rgb.b);      // F
    rgb_matrix_set_color(53, rgb.r, rgb.g, rgb.b);      // J
    rgb_matrix_set_color(72, rgb.r, rgb.g, rgb.b);      // up

    return rgb_matrix_check_finished_leds(led_max);
}

static bool battery_link_status(effect_params_t* params) {
    RGB_MATRIX_USE_LIMITS(led_min, led_max);

    uint8_t battery = MIN(dev_info.rf_baterry, 100);
    uint8_t tens_led = battery >= 10 ? battery / 10 : 0;
    uint8_t ones_led = battery % 10 == 0 ? 26 : 16 + battery % 10;
    uint8_t link_led = 32 + MIN(dev_info.link_mode, LINK_USB);

    for (uint8_t i = led_min; i < led_max && i < HGJAZHGJ_KEY_LED_COUNT; i++) {
        bool enabled = (tens_led != 0 && i == tens_led) || i == ones_led || i == link_led;
        rgb_matrix_set_color(i, enabled ? 0xFF : 0x00, enabled ? 0xFF : 0x00, enabled ? 0xFF : 0x00);
    }

    return rgb_matrix_check_finished_leds(led_max);
}

#endif // RGB_MATRIX_CUSTOM_EFFECT_IMPLS
