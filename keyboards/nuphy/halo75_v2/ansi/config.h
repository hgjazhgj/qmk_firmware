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

#define RGB_MATRIX_LED_FLUSH_LIMIT  32

#define TAP_CODE_DELAY              8
#define DYNAMIC_KEYMAP_MACRO_DELAY  8
// This is the size of the EEPROM for the custom VIA-specific data
#define EECONFIG_USER_DATA_SIZE     12

// MAC_DND sends System Do Not Disturb (0x9B), above QMK's default 0x8F.
// This preserves the factory key, but includes D-pad usages seen by Steam.
#define SYSTEM_CONTROL_USAGE_MAXIMUM 0x009B

#define DEV_MODE_PIN                C0
#define SYS_MODE_PIN                C1
#define DC_BOOST_PIN                C2
#define NRF_RESET_PIN               B4
#define NRF_BOOT_PIN                B5
#define NRF_WAKEUP_PIN              C4

#define RGB_DRIVER_SDB1             C6
#define RGB_DRIVER_SDB2             C7

#define UART_DRIVER                 SD1
#define UART_TX_PIN                 B6
#define UART_TX_PAL_MODE            0
#define UART_RX_PIN                 B7
#define UART_RX_PAL_MODE            0
// Nordic transport uses 8 data bits, even parity, one stop bit (8E1).
// STM32 counts parity in the word length: M0 selects 9 bits including parity.
#define UART_CR1                    (USART_CR1_M0 | USART_CR1_PCE)

// 7-bit IS31FL3733 addresses. ADDR2 is GND on both drivers;
// ADDR1 is GND (0x50) or VCC (0x53). The QMK driver applies the write bit.
#define IS31FL3733_I2C_ADDRESS_1   0b1010000
#define IS31FL3733_I2C_ADDRESS_2   0b1010011

#define IS31FL3733_I2C_TIMEOUT     1

/* I2C Alternate function settings */
#define I2C_DRIVER                 I2CD1
#define I2C1_SCL_PIN               B8
#define I2C1_SDA_PIN               B9

#define I2C1_SCL_PAL_MODE          1
#define I2C1_SDA_PAL_MODE          1

// STM32F072 uses I2Cv2: CLOCK_SPEED / DUTY_CYCLE do not configure this bus.
// ST RM0091 section 26.4.11, table 93: nominal 400 kHz at HSI 8 MHz,
// TIMINGR = 0x00310309. Keep in sync with STM32_I2C1SW in mcuconf.h.
// See the workspace's doc/hardware_notes.md for electrical assumptions and measurement limits.
#define I2C1_TIMINGR_PRESC         0U
#define I2C1_TIMINGR_SCLDEL        3U
#define I2C1_TIMINGR_SDADEL        1U
#define I2C1_TIMINGR_SCLH          3U
#define I2C1_TIMINGR_SCLL          9U

#define RGB_MATRIX_LED_COUNT       128

#define RGB_MATRIX_DEFAULT_MODE    RGB_MATRIX_CUSTOM_hgjazhgj
#define RGB_DEFAULT_COLOUR         168

#define RGB_MATRIX_SLEEP           // turn off effects when USB suspended
#define RGB_TRIGGER_ON_KEYDOWN

#define IS31FL3733_SW_PULLUP   IS31FL3733_PUR_0K5_OHM
#define IS31FL3733_CS_PULLDOWN IS31FL3733_PDR_0K5_OHM
