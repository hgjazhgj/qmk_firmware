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
#include "uart.h" // qmk uart.h
#include "rf.h"
#include "rf_protocol.h"
#include "side.h"
#include "sleep.h"

// The Nordic transport owns its buffers, acknowledgements and retry state.
// Other modules request operations through rf.h instead of editing these fields.
// All receive work is bounded, including a continuous stream of invalid bytes.
#define RF_RX_BUDGET 128
#define RF_RX_TIMEOUT_MS 20
#define RF_WAKE_SETTLE_US 50
#define RF_WIRE_BYTE_US 32
#define RF_REPORT_REPEATS 3
#define POWER_DOWN_DELAY 24

typedef struct {
    rf_protocol_parser_t parser;
    uint32_t             last_receive_ms;
    struct {
        uint8_t command;
        bool    waiting;
        bool    received;
        bool    read_data;
        bool    status_sync;
        bool    pairing;
        bool    handshake;
    } ack;
    struct {
        uint32_t sync_timer;
        uint16_t elapsed_ticks;
        uint8_t  lost_sync_count;
        uint8_t  disconnect_delay;
        uint8_t  previous_state;
        uint8_t  status_error_count;
        bool     reset_requested;
        bool     channel_update_requested;
    } link;
} rf_context_t;

static rf_context_t rf = {.link = {.previous_state = RF_DISCONNECT}};

static void rf_receive_frame(const rf_protocol_frame_t *frame, void *context);

static void uart_send_bytes(const uint8_t *buffer, uint16_t length, uint8_t repeats) {
    for (uint8_t i = 0; i < repeats; ++i) {
        gpio_write_pin_low(NRF_WAKEUP_PIN);
        wait_us(RF_WAKE_SETTLE_US);
        uart_transmit(buffer, length);
        // QMK's sdWrite queues bytes; keep the Nordic awake until the final
        // stop bit has left the UART. Preserve the manufacturer's timing margin.
        wait_us(RF_WAKE_SETTLE_US + length * RF_WIRE_BYTE_US);
        gpio_write_pin_high(NRF_WAKEUP_PIN);
        if (repeats > 1) wait_us(200);
    }
}

// Commands use a payload sum; names, reports and battery configuration also XOR
// the header. Keep this protocol distinction explicit rather than duplicating
// manually computed checksum bytes across each command.
static void rf_send_payload(uint8_t command, uint8_t flags, const uint8_t *payload, uint8_t length, bool xor_header, uint8_t repeats) {
    uint8_t frame[85] = {RF_PROTOCOL_HEADER, command, flags, length}; // 80-byte battery configuration + framing
    if (length > sizeof(frame) - 5 || (length && !payload)) return;
    uint8_t checksum = 0;
    for (uint8_t i = 0; i < length; ++i) {
        frame[4 + i] = payload[i];
        checksum += payload[i];
    }
    frame[4 + length] = xor_header ? checksum ^ RF_PROTOCOL_HEADER : checksum;
    uart_send_bytes(frame, length + 5, repeats);
}

void rf_request_channel_update(void) {
    rf.link.channel_update_requested = true;
}

bool rf_pairing_acknowledged(void) {
    return rf.ack.pairing;
}

uint16_t rf_link_elapsed_ticks(void) {
    return rf.link.elapsed_ticks;
}

void rf_reset_link_timer(void) {
    rf.link.elapsed_ticks = 0;
}

void rf_tick(void) {
    if (rf.link.elapsed_ticks < UINT16_MAX) {
        rf.link.elapsed_ticks++;
    }
}

// A checksum-valid frame is still not necessarily a complete command response.
// Validate the command payload before updating state or acknowledging a request.
static void rf_receive_frame(const rf_protocol_frame_t *frame, void *context) {
    (void)context;
    const uint8_t *data = frame->payload;
    switch (frame->command) {
        case CMD_HAND:
            rf.ack.handshake = true;
            break;
        case CMD_NEW_ADV:
            rf.ack.pairing = true;
            break;
        case CMD_24G_SUSPEND:
            sleep_request();
            break;
        case CMD_RF_STS_SYSC:
            // A short ACK may acknowledge the request, but contains no telemetry.
            if (!frame->is_ack) {
                if (frame->payload_length < 5) return;
                if (data[0] > LINK_USB) return;
                if (data[1] > RF_SNIF && data[1] != RF_INVALID && data[1] != RF_ERR_STATE) return;
                rf.ack.status_sync = true;
                if (device_state.link_mode == data[0]) {
                    rf.link.status_error_count = 0;
                    device_state.rf_state      = data[1];
                    if (data[1] == RF_CONNECT && (data[2] & 0xf8) == 0) device_state.keyboard_leds = data[2];
                    device_state.charge_state    = data[3];
                    device_state.battery_percent = data[4];
                } else if (device_state.rf_state != RF_INVALID) {
                    if (++rf.link.status_error_count >= 6) {
                        rf.link.status_error_count = 0;
                        rf_request_channel_update();
                    }
                }
            }
            break;
        case CMD_READ_DATA:
            if (!frame->is_ack) {
                if (frame->payload_length < RF_CONFIG_DATA_SIZE) return;
                if (data[4] <= LINK_USB) device_state.link_mode = data[4];
                if (data[5] < LINK_USB) device_state.rf_channel = data[5];
                if (data[6] >= LINK_BT_1 && data[6] <= LINK_BT_3) device_state.ble_channel = data[6];
                rf.ack.read_data = true;
            }
            break;
        case CMD_SLEEP:
        case CMD_SET_LINK:
        case CMD_CLR_DEVICE:
        case CMD_SET_CONFIG:
        case CMD_SET_NAME:
        case CMD_SET_24G_NAME:
        case CMD_RF_DFU:
            break;
        default:
            return;
    }
    // Preserve the manufacturer's liveness rule: any valid supported reply
    // proves the module is responding, including ACKs without telemetry.
    rf.link.lost_sync_count = 0;
    rf.link.reset_requested = false;
    if (rf.ack.waiting && frame->command == rf.ack.command) rf.ack.received = true;
}

/**
 * @brief  Uart send cmd.
 * @param  cmd: cmd.
 * @param  wait_ack: wait time for ack after sending.
 * @param  delayms: delay before sending.
 */
uint8_t uart_send_cmd(uint8_t cmd, uint8_t wait_ack, uint8_t delayms) {
    uint8_t payload[46] = {0};
    uint8_t length      = 1;
    bool    xor_header  = false;
    switch (cmd) {
        case CMD_SLEEP:
        case CMD_HAND:
        case CMD_CLR_DEVICE:
        case CMD_RF_DFU:
            break;
        case CMD_RF_STS_SYSC:
        case CMD_SET_LINK:
            payload[0] = device_state.link_mode;
            break;
        case CMD_NEW_ADV:
            payload[0] = device_state.link_mode;
            payload[1] = 1;
            length     = 2;
            break;
        case CMD_SET_CONFIG:
            payload[0] = POWER_DOWN_DELAY;
            break;
        case CMD_READ_DATA:
            payload[1] = RF_CONFIG_DATA_SIZE;
            length     = 2;
            break;
        case CMD_SET_NAME: {
            static const char name[] = "NuPhy Halo75 V2-";
            payload[0]               = 1;
            payload[1]               = 16;
            memcpy(payload + 2, name, sizeof(name) - 1);
            length     = 18;
            xor_header = true;
            break;
        }
        case CMD_SET_24G_NAME: {
            static const char name[] = "NuPhy Halo75 V2 Dongle";
            payload[0]               = 46;
            payload[1]               = 3;
            for (uint8_t i = 0; i < sizeof(name) - 1; ++i)
                payload[2 + i * 2] = name[i];
            length     = 46;
            xor_header = true;
            break;
        }
        default:
            return TX_DATA_ERR;
    }

    wait_ms(delayms);
    // Process bytes queued before this transaction before arming its ACK flag.
    rf_receive_task();
    if (uart_available()) return TX_BUSY;
    // The protocol has no sequence numbers. Do not let a buffered prefix from
    // a previous transaction become this command's ACK when its tail arrives.
    rf_protocol_reset(&rf.parser);
    rf.ack.command  = cmd;
    rf.ack.received = false;
    rf.ack.waiting  = wait_ack != 0;
    if (cmd == CMD_HAND) rf.ack.handshake = false;
    if (cmd == CMD_READ_DATA) rf.ack.read_data = false;
    if (cmd == CMD_RF_STS_SYSC) rf.ack.status_sync = false;
    if (cmd == CMD_SET_LINK || cmd == CMD_NEW_ADV) {
        device_state.rf_state    = cmd == CMD_NEW_ADV ? RF_PAIRING : RF_LINKING;
        rf.link.elapsed_ticks    = 0;
        rf.link.disconnect_delay = UINT8_MAX;
        if (cmd == CMD_NEW_ADV) rf.ack.pairing = false;
    }
    // A queued configuration reply may have selected a different channel while
    // draining RX; encode the same mode that the board now considers current.
    if (cmd == CMD_RF_STS_SYSC || cmd == CMD_SET_LINK || cmd == CMD_NEW_ADV) payload[0] = device_state.link_mode;
    rf_send_payload(cmd, 0, payload, length, xor_header, 1);

    for (uint8_t elapsed = 0; elapsed < wait_ack; ++elapsed) {
        wait_ms(1);
        rf_receive_task();
        if (rf.ack.received) break;
    }
    rf.ack.waiting = false;
    return !wait_ack || rf.ack.received ? TX_OK : TX_TIMEOUT;
}

/**
 * @brief RF module state sync.
 */
void rf_link_task(void) {
    if (timer_elapsed32(rf.link.sync_timer) < 200)
        return;
    else
        rf.link.sync_timer = timer_read32();

    if (rf.link.reset_requested) {
        rf.link.reset_requested = 0;
        wait_ms(100);
        gpio_write_pin_low(NRF_RESET_PIN);
        wait_ms(50);
        // Nothing received before the reset can acknowledge the restarted radio.
        for (uint16_t i = 0; i < RF_RX_BUDGET && uart_available(); ++i)
            uart_read();
        rf_protocol_reset(&rf.parser);
        memset(&rf.ack, 0, sizeof(rf.ack));
        gpio_write_pin_high(NRF_RESET_PIN);
        wait_ms(50);
        device_state.rf_state = RF_IDLE;
    } else if (rf.link.channel_update_requested) {
        rf.link.channel_update_requested = 0;
        uart_send_cmd(CMD_SET_LINK, 10, 10);
    }

    if (device_state.link_mode == LINK_USB) {
        if (board_host_mode() != HOST_USB_TYPE) {
            board_set_host_mode(HOST_USB_TYPE);
            board_release_keys();
        }
        side_set_link_blinks(0);
    } else {
        if (board_host_mode() != HOST_RF_TYPE) {
            // Release USB reports before routing subsequent reports to RF.
            board_release_keys();
            board_set_host_mode(HOST_RF_TYPE);
        }

        if (device_state.rf_state != RF_CONNECT) {
            if (rf.link.disconnect_delay >= 10) {
                side_set_link_blinks(3);
                side_restart_link_indicator();
                rf.link.previous_state = device_state.rf_state;
            } else {
                rf.link.disconnect_delay++;
            }
        } else if (device_state.rf_state == RF_CONNECT) {
            rf.link.elapsed_ticks    = 0;
            rf.link.disconnect_delay = 0;
            side_set_link_blinks(0);

            if (rf.link.previous_state != RF_CONNECT) {
                rf.link.previous_state = RF_CONNECT;
                side_restart_link_indicator();
                if (device_state.link_mode == LINK_RF_24) {
                    uart_send_cmd(CMD_SET_24G_NAME, 10, 30);
                }
            }
        }
    }

    if (device_state.link_mode != LINK_USB) rf.link.lost_sync_count++;
    uart_send_cmd(CMD_RF_STS_SYSC, 1, 1);

    if (device_state.link_mode != LINK_USB) {
        if (rf.link.lost_sync_count >= 5) {
            rf.link.lost_sync_count = 0;
            rf.link.reset_requested = 1;
        }
    }
}

#define BAT_CFG_LEN 80
static const uint8_t battery_acfg_tab[BAT_CFG_LEN] = {
    0x50, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xB4, 0xC2, 0xB4, 0xA8, 0x9B, 0x96, 0xF8, 0xF2, 0xF3, 0xC3, 0xA8, 0x8A, 0x65, 0x55, 0x49, 0x41, 0x39, 0x34, 0x2E, 0xA9, 0xAE, 0xD3, 0x28, 0xFF, 0xFF, 0xF1, 0xD3, 0xCE, 0xCB, 0xC8, 0xC3, 0xB8, 0xAE, 0xA7, 0xA8, 0xA6, 0x82, 0x6D, 0x65, 0x63, 0x69, 0x79, 0x8D, 0xA4, 0xB7, 0xC8, 0xA4, 0x16, 0x20, 0x00, 0xA7, 0x10, 0x00, 0xB1, 0x28, 0x00, 0x00, 0x00, 0x64, 0x43, 0xC0, 0x53, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x81,
};

static void uart_send_battery_config(void) {
    rf_send_payload(CMD_WBAT_CFG, 1, battery_acfg_tab, sizeof(battery_acfg_tab), true, 1);
    wait_ms(50);
}

void uart_send_report(uint8_t report_type, const uint8_t *report_buf, uint8_t report_size) {
    if (!board_dial_is_ready() || device_state.link_mode == LINK_USB || device_state.rf_state != RF_CONNECT) return;
    // Restrict each public report entry to its actual wire layout.
    uint8_t expected;
    switch (report_type) {
        case CMD_RPT_BYTE_KB:
            expected = 8;
            break;
        case CMD_RPT_BIT_KB:
            expected = 16;
            break;
        case CMD_RPT_MS:
            expected = 5;
            break;
        case CMD_RPT_CONSUME:
        case CMD_RPT_SYS:
            expected = 2;
            break;
        default:
            return;
    }
    if (!report_buf || report_size != expected) return;
    rf_send_payload(report_type, 1, report_buf, report_size, true, RF_REPORT_REPEATS);
    wait_us(200);
}

void rf_receive_task(void) {
    uint16_t remaining = RF_RX_BUDGET;
    bool     received  = false;
    while (remaining-- && uart_available()) {
        rf_protocol_receive(&rf.parser, uart_read(), rf_receive_frame, NULL);
        received = true;
    }
    if (received) {
        rf.last_receive_ms = timer_read32();
    } else if (rf_protocol_pending(&rf.parser) && timer_elapsed32(rf.last_receive_ms) >= RF_RX_TIMEOUT_MS) {
        // A long scheduling pause alone is not a wire gap: consume queued bytes
        // first. An incomplete frame expires only on a later empty poll.
        rf_protocol_expire(&rf.parser, rf_receive_frame, NULL);
    }
}

/**
 * @brief  RF uart initial.
 */
void rf_uart_init(void) {
    /* Set UART baud rate to 460800. */
    uart_init(460800);

    // Parity is configured by UART_CR1 before sdStart (see config.h).
    // Configure the complete pin mode so an old pull-down cannot survive ORing.
    palSetLineMode(UART_TX_PIN, PAL_MODE_ALTERNATE(UART_TX_PAL_MODE) | PAL_OUTPUT_TYPE_PUSHPULL | PAL_STM32_OSPEED_LOWEST | PAL_STM32_PUPDR_PULLUP);
    palSetLineMode(UART_RX_PIN, PAL_MODE_ALTERNATE(UART_RX_PAL_MODE) | PAL_OUTPUT_TYPE_PUSHPULL | PAL_STM32_OSPEED_LOWEST | PAL_STM32_PUPDR_PULLUP);
}

/**
 * @brief RF module initial.
 */
void rf_device_init(void) {
    uint8_t timeout = 0;

    timeout          = 10;
    rf.ack.handshake = 0;
    while (timeout--) {
        uart_send_cmd(CMD_HAND, 0, 20);
        wait_ms(5);
        rf_receive_task();
        if (rf.ack.handshake) break;
    }

    timeout          = 10;
    rf.ack.read_data = 0;
    while (timeout--) {
        uart_send_cmd(CMD_READ_DATA, 0, 20);
        wait_ms(5);
        rf_receive_task();
        if (rf.ack.read_data) break;
    }

    timeout            = 10;
    rf.ack.status_sync = 0;
    while (timeout--) {
        uart_send_cmd(CMD_RF_STS_SYSC, 0, 20);
        wait_ms(5);
        rf_receive_task();
        if (rf.ack.status_sync) break;
    }

    uart_send_battery_config();

    uart_send_cmd(CMD_SET_NAME, 10, 20);

    uart_send_cmd(CMD_SET_24G_NAME, 10, 20);
}
