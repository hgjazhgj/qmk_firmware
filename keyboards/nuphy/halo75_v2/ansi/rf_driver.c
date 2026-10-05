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

#include "rf_driver.h"
#include "board_runtime.h"
#include "rf_reports.h"

/* Host driver */
static uint8_t rf_keyboard_leds(void);
static void    rf_send_keyboard(report_keyboard_t *report);
static void    rf_send_nkro(report_nkro_t *report);
static void    rf_send_mouse(report_mouse_t *report);
static void    rf_send_extra(report_extra_t *report);
host_driver_t  rf_host_driver = {rf_keyboard_leds, rf_send_keyboard, rf_send_nkro, rf_send_mouse, rf_send_extra};

static uint8_t rf_keyboard_leds(void) {
    return device_state.keyboard_leds;
}

static void rf_send_keyboard(report_keyboard_t *report) {
    uart_send_report_keyboard(report);
}

static void rf_send_nkro(report_nkro_t *report) {
    uart_send_report_nkro(report);
}

static void rf_send_mouse(report_mouse_t *report) {
    uart_send_mouse_report(report);
}

static void rf_send_extra(report_extra_t *report) {
    if (report->report_id == REPORT_ID_CONSUMER) {
        uart_send_consumer_report(report);
    } else if (report->report_id == REPORT_ID_SYSTEM) {
        uart_send_system_report(report);
    }
}
