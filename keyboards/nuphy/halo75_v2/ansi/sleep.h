// Copyright 2023 @ Nuphy <https://nuphy.com/>
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <stdbool.h>

// A wireless request is evaluated against activity and the current transport.
void sleep_request(void);
bool sleep_is_active(void);

// A pressed key may request one USB remote-wakeup signal when the host permits.
// Call before user key processing so a consumed key can still wake the host.
void sleep_note_keypress(void);

// Apply the lighting state immediately; also clears any pending sleep request.
void sleep_set_active(bool sleeping);
void sleep_task(void);
