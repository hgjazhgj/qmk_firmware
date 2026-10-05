// Copyright 2023 @ Nuphy <https://nuphy.com/>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "quantum.h"
#include "board_runtime.h"
#include "rf.h"
#include "rf_reports.h"

// These are Nordic protocol sizes, independent of QMK's USB report layout.
enum {
    RF_BYTE_REPORT_SIZE  = 8,
    RF_BYTE_KEY_OFFSET   = 2,
    RF_BIT_REPORT_SIZE   = 16,
    RF_BIT_KEY_COUNT     = (RF_BIT_REPORT_SIZE - 1) * 8,
    RF_RESEND_MS         = 300,
    RF_RESEND_IDLE_TICKS = 2000,
};

typedef struct {
    uint32_t resend_timer;
    uint8_t  bytes[RF_BYTE_REPORT_SIZE];
    uint8_t  bits[RF_BIT_REPORT_SIZE];
    bool     bits_active;
} rf_report_context_t;

static rf_report_context_t reports;

static bool key_is_held(const report_nkro_t *report, uint16_t key) {
    return key != 0 && key / 8 < sizeof(report->bits) && (report->bits[key / 8] & (1u << (key % 8)));
}

static bool byte_report_has_key(const uint8_t *bytes, uint8_t key) {
    for (uint8_t slot = RF_BYTE_KEY_OFFSET; slot < RF_BYTE_REPORT_SIZE; ++slot) {
        if (bytes[slot] == key) return true;
    }
    return false;
}

static bool byte_report_add_key(uint8_t *bytes, uint8_t key) {
    for (uint8_t slot = RF_BYTE_KEY_OFFSET; slot < RF_BYTE_REPORT_SIZE; ++slot) {
        if (bytes[slot] == 0) {
            bytes[slot] = key;
            return true;
        }
    }
    return false;
}

static void update_bit_report(const uint8_t *bits) {
    if (memcmp(reports.bits, bits, sizeof(reports.bits)) != 0) {
        memcpy(reports.bits, bits, sizeof(reports.bits));
        // Keep resending empty overflow reports too, so a lost release can recover.
        reports.bits_active = true;
        uart_send_report(CMD_RPT_BIT_KB, reports.bits, sizeof(reports.bits));
    }
}

void rf_clear_keyboard_reports(void) {
    memset(reports.bytes, 0, sizeof(reports.bytes));
    memset(reports.bits, 0, sizeof(reports.bits));
    // Do not clear bits_active: the next periodic report must repeat the release.
}

void rf_reports_task(void) {
    if (device_state.link_mode == LINK_USB || timer_elapsed32(reports.resend_timer) <= RF_RESEND_MS) return;

    reports.resend_timer = timer_read32();
    if (board_idle_ticks() <= RF_RESEND_IDLE_TICKS) {
        uart_send_report(CMD_RPT_BYTE_KB, reports.bytes, sizeof(reports.bytes));
        wait_us(200);
        if (reports.bits_active) uart_send_report(CMD_RPT_BIT_KB, reports.bits, sizeof(reports.bits));
    } else {
        reports.bits_active = false;
    }
}

static void send_usage(uint8_t command, uint16_t usage) {
    const uint8_t payload[] = {(uint8_t)usage, (uint8_t)(usage >> 8)};
    board_note_activity();
    uart_send_report(command, payload, sizeof(payload));
}

void uart_send_consumer_report(const report_extra_t *report) {
    send_usage(CMD_RPT_CONSUME, report->usage);
}

void uart_send_system_report(const report_extra_t *report) {
    send_usage(CMD_RPT_SYS, report->usage);
}

static uint8_t mouse_axis_byte(int16_t value) {
    // The module accepts signed 8-bit axes even with QMK extended mouse reports.
    if (value > INT8_MAX) value = INT8_MAX;
    if (value < INT8_MIN) value = INT8_MIN;
    return (uint8_t)value;
}

void uart_send_mouse_report(const report_mouse_t *report) {
    const uint8_t payload[] = {report->buttons, mouse_axis_byte(report->x), mouse_axis_byte(report->y), mouse_axis_byte(report->v), mouse_axis_byte(report->h)};
    board_note_activity();
    uart_send_report(CMD_RPT_MS, payload, sizeof(payload));
}

void uart_send_report_keyboard(const report_keyboard_t *report) {
    const uint8_t empty_bits[RF_BIT_REPORT_SIZE] = {0};
    board_note_activity();
    // Leaving NKRO must release its separate overflow report as well.
    update_bit_report(empty_bits);
    reports.bytes[0] = report->mods;
    reports.bytes[1] = 0;
    memcpy(reports.bytes + RF_BYTE_KEY_OFFSET, report->keys, RF_BYTE_REPORT_SIZE - RF_BYTE_KEY_OFFSET);
    uart_send_report(CMD_RPT_BYTE_KB, reports.bytes, sizeof(reports.bytes));
}

void uart_send_report_nkro(const report_nkro_t *report) {
    uint8_t bytes[RF_BYTE_REPORT_SIZE] = {report->mods};
    uint8_t bits[RF_BIT_REPORT_SIZE]   = {0};
    board_note_activity();

    // Retain held byte slots, releasing all old keys before allocating new ones.
    // This also handles a preceding 6KRO report without duplicating held keys.
    for (uint8_t slot = RF_BYTE_KEY_OFFSET; slot < RF_BYTE_REPORT_SIZE; ++slot) {
        uint8_t key = reports.bytes[slot];
        if (key_is_held(report, key) && !byte_report_has_key(bytes, key)) bytes[slot] = key;
    }

    // Keep held overflow keys in that report. Moving a held key between the two
    // reports would require assumptions about the Nordic module's merge behavior.
    // Values >= 120 only fit in the six byte slots; a seventh such key cannot be
    // represented. Retry unrepresented keys on later calls when a slot is freed.
    // The keycode cap also prevents wrapping if QMK grows its NKRO bitmap.
    for (uint16_t key = 1; key < sizeof(report->bits) * 8 && key <= UINT8_MAX; ++key) {
        if (!key_is_held(report, key) || byte_report_has_key(bytes, (uint8_t)key)) continue;

        bool was_overflow = key < RF_BIT_KEY_COUNT && (reports.bits[1 + key / 8] & (1u << (key % 8)));
        if (!was_overflow && byte_report_add_key(bytes, (uint8_t)key)) continue;
        if (key < RF_BIT_KEY_COUNT) bits[1 + key / 8] |= 1u << (key % 8);
    }

    // Match the manufacturer ordering and modifier convention: overflow first,
    // with its modifier byte zero; modifiers belong to the byte report only.
    update_bit_report(bits);
    if (memcmp(reports.bytes, bytes, sizeof(reports.bytes)) != 0) {
        memcpy(reports.bytes, bytes, sizeof(reports.bytes));
        uart_send_report(CMD_RPT_BYTE_KB, reports.bytes, sizeof(reports.bytes));
    }
}
