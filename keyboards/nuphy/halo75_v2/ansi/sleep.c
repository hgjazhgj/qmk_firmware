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
#include "rf.h"
#include "sleep.h"
#include "hal_usb.h"
#include "usb_main.h"

// USB device GET_STATUS bit 1 reports the host-enabled remote-wakeup feature.
// QMK's similarly named constant is private to its ChibiOS protocol source.
#define BOARD_USB_REMOTE_WAKEUP_STATUS_MASK 0x02U

typedef struct {
    bool     sleeping;
    bool     sleep_requested;
    uint32_t step_timer;
    uint8_t  usb_suspend_debounce;
    uint32_t rf_disconnect_ticks;
    bool     usb_wakeup_pending;
    uint32_t usb_wakeup_timer;
} sleep_context_t;

static sleep_context_t sleep_context;

void sleep_request(void) {
    sleep_context.sleep_requested = true;
}

bool sleep_is_active(void) {
    return sleep_context.sleeping;
}

static bool usb_remote_wakeup_allowed(void) {
    return device_state.link_mode == LINK_USB && USB_DRIVER.state == USB_SUSPENDED && (USB_DRIVER.status & BOARD_USB_REMOTE_WAKEUP_STATUS_MASK);
}

void sleep_note_keypress(void) {
    if (!usb_remote_wakeup_allowed()) {
        sleep_context.usb_wakeup_pending = false;
    } else if (!sleep_context.usb_wakeup_pending) {
        sleep_context.usb_wakeup_pending = true;
        sleep_context.usb_wakeup_timer   = timer_read32();
    }
}

static void service_usb_wakeup(void) {
    if (!usb_remote_wakeup_allowed()) {
        sleep_context.usb_wakeup_pending = false;
        return;
    }
    // The key was pressed while already suspended. Waiting another 5 ms
    // guarantees the minimum idle time without delaying the keyboard task.
    if (sleep_context.usb_wakeup_pending && timer_elapsed32(sleep_context.usb_wakeup_timer) >= 5) {
        sleep_context.usb_wakeup_pending = false;
        // The HAL checks suspend again and generates the bounded resume pulse.
        usbWakeupHost(&USB_DRIVER);
    }
}

void sleep_set_active(bool sleeping) {
    sleep_context.sleeping        = sleeping;
    sleep_context.sleep_requested = false;
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
    sleep_set_active(device_state.link_mode == LINK_USB || sleep_is_active());
    suspend_power_down_user();
}

void suspend_wakeup_init_kb(void) {
    sleep_context.usb_wakeup_pending = false;
    sleep_set_active(device_state.link_mode != LINK_USB && sleep_is_active());
    suspend_wakeup_init_user();
}

/**
 * @brief  Sleep Handle.
 */
void sleep_task(void) {
    // NO_USB_STARTUP_CHECK keeps RF scanning alive and skips QMK's blocking
    // suspend loop, so this board handles a pressed key's remote wake request.
    service_usb_wakeup();

    /* 50ms interval */
    if (timer_elapsed32(sleep_context.step_timer) < 50) return;
    sleep_context.step_timer = timer_read32();

    if (device_state.link_mode == LINK_USB) {
        // Ignore wireless sleep requests; only an actual USB suspend turns lights off.
        sleep_context.rf_disconnect_ticks = 0;
        if (board_settings_sleep_enabled() && USB_DRIVER.state == USB_SUSPENDED) {
            if (sleep_context.usb_suspend_debounce < 20) sleep_context.usb_suspend_debounce++;
        } else {
            sleep_context.usb_suspend_debounce = 0;
        }
        sleep_set_active(sleep_context.usb_suspend_debounce >= 20);
        return;
    }

    sleep_context.usb_suspend_debounce = 0;
    // Cancel a pending sleep before acting on it when a key or mode switch was used.
    if (!board_settings_sleep_enabled() || board_idle_ticks() < 10) {
        sleep_context.rf_disconnect_ticks = 0;
        sleep_set_active(false);
        return;
    }

    if (sleep_context.sleeping) {
        sleep_context.sleep_requested = false;
        return;
    }
    if (device_state.link_mode != LINK_RF_24) sleep_context.sleep_requested = false;
    if (device_state.rf_state != RF_DISCONNECT) sleep_context.rf_disconnect_ticks = 0;

    if (device_state.rf_state == RF_CONNECT) {
        if (board_idle_ticks() >= SLEEP_TIME_DELAY) {
            sleep_request();
        }
    } else if (rf_link_elapsed_ticks() >= LINK_TIMEOUT && board_idle_ticks() >= LINK_TIMEOUT) {
        rf_reset_link_timer();
        sleep_request();
    } else if (device_state.rf_state == RF_DISCONNECT) {
        sleep_context.rf_disconnect_ticks++;
        if (sleep_context.rf_disconnect_ticks > 5 * 20) {
            sleep_context.rf_disconnect_ticks = 0;
            sleep_request();
        }
    }

    if (sleep_context.sleep_requested) sleep_set_active(true);
}
