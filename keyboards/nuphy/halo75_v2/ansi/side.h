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
#pragma once

#include <stdbool.h>
#include <stdint.h>

// Load and normalize preferences through the board settings service.
void side_load_settings(void);

// Called from the board's 10 ms tick and from wireless / switch events.
void    side_tick(void);
void    side_set_link_blinks(uint8_t count);
void    side_restart_link_indicator(void);
void    side_notify_system_change(void);
void    side_notify_sleep_change(void);
bool    side_is_low_battery(void);
uint8_t side_get_brightness(void);

void side_adjust_brightness(uint8_t brighten);
void side_adjust_speed(uint8_t fast);
void side_cycle_colour(uint8_t dir);
void side_cycle_effect(uint8_t dir);
void side_cycle_regions(uint8_t dir);

void side_task(void);
void side_show_reset(void);
void side_reset_settings(void);
void side_show_test(void);
