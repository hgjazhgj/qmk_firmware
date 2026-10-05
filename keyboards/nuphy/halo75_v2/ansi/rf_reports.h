// Copyright 2023 @ Nuphy <https://nuphy.com/>
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "report.h"

// Nordic report conversion and release retransmission, separate from framing.
void rf_reports_task(void);
void rf_clear_keyboard_reports(void);

void uart_send_report_keyboard(const report_keyboard_t *report);
void uart_send_report_nkro(const report_nkro_t *report);
void uart_send_mouse_report(const report_mouse_t *report);
void uart_send_consumer_report(const report_extra_t *report);
void uart_send_system_report(const report_extra_t *report);
