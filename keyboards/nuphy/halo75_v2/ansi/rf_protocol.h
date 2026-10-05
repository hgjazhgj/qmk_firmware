// Copyright 2026
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <stdbool.h>
#include <stdint.h>

#define RF_PROTOCOL_HEADER 0x5A
#define RF_PROTOCOL_ACK 0xA0
#define RF_PROTOCOL_MAX_FRAME_SIZE 64
#define RF_PROTOCOL_MAX_PAYLOAD_SIZE (RF_PROTOCOL_MAX_FRAME_SIZE - 5)

// All-zero initialization is valid. Treat these fields as private parser state.
typedef struct {
    uint8_t buffer[RF_PROTOCOL_MAX_FRAME_SIZE];
    uint8_t length;
} rf_protocol_parser_t;

typedef struct {
    uint8_t        command;
    uint8_t        flags;
    uint8_t        payload_length;
    const uint8_t *payload;
    bool           is_ack;
} rf_protocol_frame_t;

// The frame and payload are valid only during this callback. A handler must not
// recursively feed/reset the same parser. ACK frames have no payload (NULL).
typedef void (*rf_protocol_handler_t)(const rf_protocol_frame_t *frame, void *context);

void rf_protocol_reset(rf_protocol_parser_t *parser);
bool rf_protocol_pending(const rf_protocol_parser_t *parser);

// Feed one byte from the Nordic UART. Frame boundaries come from the wire
// format: [5A, command, A0] for a short ACK, otherwise
// [5A, command, flags, length, payload..., payload_sum]. An empty UART queue
// does not end a frame. No allocation or QMK HAL is required. The handler gets
// complete ACKs and checksum-valid data frames; command validation is its job.
void rf_protocol_receive(rf_protocol_parser_t *parser, uint8_t byte, rf_protocol_handler_t handler, void *context);

// Use after the caller's receive timeout, not merely when the queue is empty.
// Discard the stalled candidate and salvage any complete frames buffered behind
// it. Remaining incomplete candidates are discarded as well. The caller owns
// the clock because a delay in polling does not itself prove a gap on the wire.
void rf_protocol_expire(rf_protocol_parser_t *parser, rf_protocol_handler_t handler, void *context);
