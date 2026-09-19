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
#include "hal_usb.h"
#include "usb_main.h"

extern user_config_t    user_config;
extern DEV_INFO_STRUCT  dev_info;
extern uint16_t         rf_linking_time;
extern uint16_t         no_act_time;

extern bool             f_wakeup_prepare;
extern bool             f_goto_sleep;

void set_sleep_state(bool sleeping) {
    f_wakeup_prepare = sleeping;
    f_goto_sleep = false;
    // Keep BOOST powered as in the original light-only idle path. SDB blanks
    // the LEDs without a power cycle before the next RGB/I2C update.
    if (sleeping) {
        // Clear the LED buffers before shutting down the drivers.
        rgb_matrix_set_suspend_state(true);
        gpio_write_pin_low(RGB_DRIVER_SDB1);
        gpio_write_pin_low(RGB_DRIVER_SDB2);
    } else {
        gpio_write_pin_high(RGB_DRIVER_SDB1);
        gpio_write_pin_high(RGB_DRIVER_SDB2);
        rgb_matrix_set_suspend_state(false);
    }
}

void suspend_power_down_kb(void) {
    // USB suspend/resume must preserve the lighting state in wireless mode.
    set_sleep_state(dev_info.link_mode == LINK_USB || f_wakeup_prepare);
}

void suspend_wakeup_init_kb(void) {
    set_sleep_state(dev_info.link_mode != LINK_USB && f_wakeup_prepare);
}

/**
 * @brief  Sleep Handle.
 */
void Sleep_Handle(void) {
    static uint32_t delay_step_timer = 0;
    static uint8_t  usb_suspend_debounce = 0;
    static uint32_t rf_disconnect_time = 0;

    /* 50ms interval */
    if (timer_elapsed32(delay_step_timer) < 50) return;
    delay_step_timer = timer_read32();

    if (dev_info.link_mode == LINK_USB) {
        // Ignore wireless sleep requests; only an actual USB suspend turns lights off.
        rf_disconnect_time = 0;
        if (f_dev_sleep_enable && USB_DRIVER.state == USB_SUSPENDED) {
            if (usb_suspend_debounce < 20) usb_suspend_debounce++;
        } else {
            usb_suspend_debounce = 0;
        }
        set_sleep_state(usb_suspend_debounce >= 20);
        return;
    }

    usb_suspend_debounce = 0;
    // Cancel a pending sleep before acting on it when a key or mode switch was used.
    if (!f_dev_sleep_enable || no_act_time < 10) {
        rf_disconnect_time = 0;
        set_sleep_state(false);
        return;
    }

    if (f_wakeup_prepare) {
        f_goto_sleep = false;
        return;
    }
    if (dev_info.link_mode != LINK_RF_24) f_goto_sleep = false;
    if (dev_info.rf_state != RF_DISCONNECT) rf_disconnect_time = 0;

    if (dev_info.rf_state == RF_CONNECT) {
        if (no_act_time >= SLEEP_TIME_DELAY) {
            f_goto_sleep = true;
        }
    } else if (rf_linking_time >= LINK_TIMEOUT && no_act_time >= LINK_TIMEOUT) {
        rf_linking_time = 0;
        f_goto_sleep = true;
    } else if (dev_info.rf_state == RF_DISCONNECT) {
        rf_disconnect_time++;
        if (rf_disconnect_time > 5 * 20) {
            rf_disconnect_time = 0;
            f_goto_sleep = true;
        }
    }

    if (f_goto_sleep) set_sleep_state(true);
}
