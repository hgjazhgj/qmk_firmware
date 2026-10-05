// Copyright 2023 @ Nuphy <https://nuphy.com/>
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <stdbool.h>
#include <stdint.h>

// Nordic command values; keep the manufacturer's wire format unchanged.
enum {
    CMD_SLEEP        = 0xF1,
    CMD_HAND         = 0xF2,
    CMD_24G_SUSPEND  = 0xF4,
    CMD_RPT_MS       = 0xE0,
    CMD_RPT_BYTE_KB  = 0xE1,
    CMD_RPT_BIT_KB   = 0xE2,
    CMD_RPT_CONSUME  = 0xE3,
    CMD_RPT_SYS      = 0xE4,
    CMD_SET_LINK     = 0xC0,
    CMD_SET_CONFIG   = 0xC1,
    CMD_SET_NAME     = 0xC3,
    CMD_CLR_DEVICE   = 0xC5,
    CMD_NEW_ADV      = 0xC7,
    CMD_RF_STS_SYSC  = 0xC9,
    CMD_SET_24G_NAME = 0xCA,
    CMD_RF_DFU       = 0xB1,
    CMD_READ_DATA    = 0x81,
    CMD_WBAT_CFG     = 0x82,
};

enum {
    TX_OK = 0xE0,
    TX_DONE,
    TX_BUSY,
    TX_TIMEOUT,
    TX_DATA_ERR,
};

#define RF_CONFIG_DATA_SIZE 32

// Board lifecycle and wireless protocol operations.
void    rf_uart_init(void);
void    rf_device_init(void);
void    rf_receive_task(void);
void    rf_link_task(void);
uint8_t uart_send_cmd(uint8_t cmd, uint8_t wait_ack, uint8_t delayms);
void    uart_send_report(uint8_t report_type, const uint8_t *report_buf, uint8_t report_size);

void rf_request_channel_update(void);
bool rf_pairing_acknowledged(void);

// Call once per 10 ms board tick; link timeout counts saturate at UINT16_MAX.
void     rf_tick(void);
uint16_t rf_link_elapsed_ticks(void);
void     rf_reset_link_timer(void);
