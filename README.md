# stm32-pcd8544

A from-scratch STM32 HAL driver for the Nokia 5110 LCD (PCD8544 controller), running on an STM32F103C8T6 "Blue Pill". It uses hardware SPI1 and no Arduino layer or third-party display library.

The demo firmware draws an 8×8-pixel checkerboard one row at a time, then erases it the same way, in an endless loop.

## Hardware

| Nokia 5110 pin | Blue Pill pin | Notes |
|---|---|---|
| VCC | 3.3V | Bare (red) breakout, no onboard regulator |
| GND | GND | |
| CLK / SCLK | PA5 | SPI1_SCK |
| DIN / MOSI | PA7 | SPI1_MOSI |
| DC | PA2 | Low = command, high = data |
| CE / SCE | PA1 | Active low, toggled around every byte |
| RST | PA0 | Active low |

The board is programmed over SWD with an ST-Link. The Blue Pill is a clone, so `platformio.ini` disables OpenOCD's CPUTAPID check.

## Driver API (Layer 1)

```c
#include "pcd8544.h"

void pcd8544_init(SPI_HandleTypeDef *hspi);  // GPIO setup, 100 ms reset pulse, init commands
void pcd8544_send_cmd(uint8_t cmd);           // DC low, CE low, 1 byte over SPI, CE high
void pcd8544_send_data(uint8_t data);         // DC high, CE low, 1 byte over SPI, CE high
```

SPI1 is set to mode 0, MSB first, 8-bit, at 2.25 MHz (72 MHz APB2 ÷ 32), below the PCD8544's 4 MHz limit.

## Build and flash

Requires [PlatformIO](https://platformio.org/).

```bash
pio run                 # build
pio run -t upload       # flash over ST-Link
```

## The interesting bug

The first build compiled and flashed, but the screen stayed blank. Lowering the SPI clock, sweeping the contrast and reseating every jumper wire all failed to fix it.

The clue was that a faint pattern briefly appeared **while the board reset and while it powered off**. That meant the display had been receiving the data all along. The contrast setting (`Vop = 0xC0`) was just outside the range where this particular panel shows anything at a steady 3.3V. As the supply voltage rose or fell, it briefly passed through the visible range.

The fix was a different init sequence (`Vop 0xB8`, temperature coefficient `0x04`, bias `0x14`) and holding RST low for 100 ms instead of 10 ms. Filling the screen solid black (`0xFF`) confirmed it worked.

A second problem appeared right after. The original 1-pixel checkerboard (`0x55`/`0xAA` per byte) is too fine for the eye to resolve, so it looks blank even when it's drawn correctly. 8×8 blocks fixed that.

The full write-up is in [DEVLOG.md](DEVLOG.md), and the rules that came out of it are in [CLAUDE.md](CLAUDE.md).

## Layout

```
lib/pcd8544/      driver (pcd8544.h, pcd8544.c)
src/main.c        clock, SPI1 setup, checkerboard animation
include/          STM32F1 HAL config
```
