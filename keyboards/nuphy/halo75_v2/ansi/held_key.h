// Copyright 2026
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    HELD_KEY_NONE,
    HELD_KEY_SHORT,
    HELD_KEY_LONG,
} held_key_result_t;

// An all-zero object is inactive. The board owns each instance; use these
// operations to change its state. Identity remains available after consumption.
typedef struct {
    uint32_t started_at;
    uint16_t identity;
    bool     active;
} held_key_t;

// Every press starts a fresh gesture, including repeated presses of one key.
// now and threshold use the same monotonic uint32_t clock units. Unsigned
// elapsed-time subtraction handles a clock wrap during a gesture.
static inline void held_key_start(held_key_t *hold, uint16_t identity, uint32_t now) {
    hold->started_at = now;
    hold->identity   = identity;
    hold->active     = true;
}

// Polling only reports a long press; the result consumes the active gesture.
static inline held_key_result_t held_key_poll(held_key_t *hold, uint32_t now, uint32_t threshold) {
    if (!hold->active || (uint32_t)(now - hold->started_at) < threshold) return HELD_KEY_NONE;
    hold->active = false;
    return HELD_KEY_LONG;
}

// Other keys' releases leave this gesture alone. A matching release consumes
// it, including a long press whose deadline passed since the preceding poll.
static inline held_key_result_t held_key_release(held_key_t *hold, uint16_t identity, uint32_t now, uint32_t threshold) {
    if (!hold->active || hold->identity != identity) return HELD_KEY_NONE;
    hold->active = false;
    return (uint32_t)(now - hold->started_at) >= threshold ? HELD_KEY_LONG : HELD_KEY_SHORT;
}
