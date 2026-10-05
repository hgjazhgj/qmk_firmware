// Copyright 2026
// SPDX-License-Identifier: GPL-2.0-or-later
#include "rf_protocol.h"

#include <stddef.h>
#include <string.h>

void rf_protocol_reset(rf_protocol_parser_t *parser) {
    parser->length = 0;
}

bool rf_protocol_pending(const rf_protocol_parser_t *parser) {
    return parser->length != 0;
}

static void discard_prefix(rf_protocol_parser_t *parser, uint8_t count) {
    parser->length -= count;
    memmove(parser->buffer, parser->buffer + count, parser->length);
}

static void parse_buffer(rf_protocol_parser_t *parser, bool expired, rf_protocol_handler_t handler, void *context) {
    while (parser->length) {
        if (parser->buffer[0] != RF_PROTOCOL_HEADER) {
            discard_prefix(parser, 1);
            continue;
        }

        if (parser->length < 3) {
            if (!expired) return;
            discard_prefix(parser, 1);
            continue;
        }

        const bool is_ack         = parser->buffer[2] == RF_PROTOCOL_ACK;
        uint8_t    payload_length = 0;
        uint8_t    frame_length   = 3;
        if (!is_ack) {
            if (parser->length < 4) {
                if (!expired) return;
                discard_prefix(parser, 1);
                continue;
            }

            payload_length = parser->buffer[3];
            if (payload_length > RF_PROTOCOL_MAX_PAYLOAD_SIZE) {
                discard_prefix(parser, 1);
                continue;
            }

            frame_length = payload_length + 5;
            if (parser->length < frame_length) {
                if (!expired) return;
                discard_prefix(parser, 1);
                continue;
            }

            // Manufacturer RX uses only the modulo-256 payload sum. Its TX
            // reports use an additional XOR with the header; do not reuse that
            // checksum here. Header/command/flags/length are not checksummed.
            uint8_t checksum = 0;
            for (uint8_t i = 0; i < payload_length; ++i) {
                checksum += parser->buffer[4 + i];
            }
            if (checksum != parser->buffer[4 + payload_length]) {
                // Keep possible headers in the rejected candidate. Restarting
                // at the first header as bytes arrive would corrupt valid data
                // whose payload itself contains 0x5A.
                discard_prefix(parser, 1);
                continue;
            }
        }

        const rf_protocol_frame_t frame = {
            .command        = parser->buffer[1],
            .flags          = parser->buffer[2],
            .payload_length = payload_length,
            .payload        = is_ack ? NULL : parser->buffer + 4,
            .is_ack         = is_ack,
        };
        handler(&frame, context);
        discard_prefix(parser, frame_length);
    }
}

void rf_protocol_receive(rf_protocol_parser_t *parser, uint8_t byte, rf_protocol_handler_t handler, void *context) {
    // parse_buffer always consumes or rejects a candidate by the time it has
    // 64 bytes, so there is always room here, including for a maximum frame.
    parser->buffer[parser->length++] = byte;
    parse_buffer(parser, false, handler, context);
}

void rf_protocol_expire(rf_protocol_parser_t *parser, rf_protocol_handler_t handler, void *context) {
    parse_buffer(parser, true, handler, context);
}
