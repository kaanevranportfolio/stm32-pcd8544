# Dev Log

## 2026-09-12 — PCD8544 Layer 1 driver bring-up

**Goal**: Bring up a native HAL driver for the Nokia 5110 (PCD8544) on a Blue Pill (STM32F103C8T6), confirmed with a checkerboard test pattern over hardware SPI1.

**Initial state**: `pio run` built successfully on the first pass (SPI1 on PA5/PA7, RST/CE/DC on PA0/PA1/PA2, standard init sequence with `Vop=0xC0`, temp coeff `0x06`, bias `0x13`, 10ms RST hold). Flashed — display stayed completely blank.

**Debugging, in the order tried:**

1. Suspected the SPI clock (was running at 9MHz via APB2/8, above the PCD8544's 4MHz datasheet cap). Lowered to 2.25MHz (APB2/32). No change — still blank.
2. Added a PC13 heartbeat LED plus a firmware routine that continuously swept the `Vop` contrast register across its full range while the LED blinked, to rule contrast and "is the MCU even running" in or out simultaneously. LED blinked correctly throughout — firmware was alive and running. Display never showed anything across the entire sweep.
3. Suspected a loose breadboard jumper (a very plausible cause on jumper-wire prototyping). Reseated all 7 wires (VCC, GND, CLK, DIN, DC, CE, RST) and re-ran the contrast sweep. Still blank.
4. New information changed the picture: the user recalled that the *very first* manual flash (before any of the above changes) actually did show the checkerboard stripes briefly. This ruled out the SPI-speed theory as the root cause and reframed the problem as something that regressed, or was marginal from the start.
5. Two more transient observations were reported: a brief blink visible during the MCU's own reset (mid-reflash), and a stripe briefly visible during power-off. Both are the real tell: DDRAM was being written correctly throughout, and the supply voltage sweeping through the panel's actual usable contrast range during power transients is what made already-correct content briefly visible. At a stable 3.3V, the configured `Vop` simply fell outside that panel's visible window.
6. Verified with the most unambiguous test possible — a full `0xFF` fill (every pixel on) — after changing `Vop` to `0xB8`, temp coefficient to `0x04`, bias to `0x14`, and holding RST low for 100ms instead of 10ms. Result: the entire glass went solid black. Driver, wiring, and SPI were confirmed fully working.
7. Restored the checkerboard test pattern on top of the now-working init — and it looked blank again. This turned out to be a second, unrelated bug in the *test*, not the driver: alternating `0x55`/`0xAA` per DDRAM byte produces a genuine 1-pixel-pitch checkerboard, which is too fine to perceive on the physical display and looks uniform/blank even when correct. Replaced it with 8×8-pixel block squares (one full page tall), which rendered as a clearly visible checkerboard.

**Root causes (two, independent):**
- Wrong `Vop`/bias/temp-coefficient values for this specific panel (fixed: `0xB8`/`0x04`/`0x14`, with a 100ms RST hold).
- The checkerboard test pattern itself was too fine-grained to be a valid visual confirmation (fixed: 8×8 pixel blocks).

**Outcome**: `pio run` succeeds, firmware flashes via ST-Link, and the display shows a clearly visible 8×8-pixel checkerboard pattern confirming SPI communication, GPIO control lines, and PCD8544 initialization are all working correctly.

See [CLAUDE.md](CLAUDE.md) → "Known Display Driver Gotchas — PCD8544 Nokia 5110" for the reusable rules extracted from this session.
