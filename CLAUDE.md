# stm32-experiments

PlatformIO STM32F103C8T6 (Blue Pill) project using the `stm32cube` framework.

## Known STM32 HAL Gotchas

1. Always call `HAL_Init()` before `SystemClock_Config()` — `HAL_Init()` initializes SysTick, which `HAL_GetTick()` depends on.
2. Always enable the GPIO clock before configuring any pin — `__HAL_RCC_GPIOC_CLK_ENABLE()` for PC13, `__HAL_RCC_GPIOA_CLK_ENABLE()` for PA pins, etc.
3. PC13 is active-low — `GPIO_PIN_RESET` = LED on, `GPIO_PIN_SET` = LED off.
4. `SystemClock_Config` must configure HSE + PLL to reach 72MHz — do not leave it at default HSI 8MHz.
5. Clone Blue Pill — CPUTAPID check disabled in `platformio.ini` (`upload_flags = -c \"set CPUTAPID 0\"`), never remove this.
6. **`stm32f1xx_it.c` (or an equivalent handler) is not optional** — the CMSIS startup file (`startup_stm32f103xb.s`) weak-aliases `SysTick_Handler` to `Default_Handler`, which is just an infinite `b .` loop. If nothing in the project defines `SysTick_Handler`, the first SysTick interrupt (fired as soon as `HAL_Init()` enables it) traps the CPU forever — before `main()`'s loop ever runs. Symptom: flash reports SUCCESS, chip resets, but nothing happens (e.g. LED stays latched in its initial state). Fix: define
   ```c
   void SysTick_Handler(void)
   {
       HAL_IncTick();
   }
   ```
   This is the actual root cause found during the blink debug session — the other four gotchas above were already handled correctly in the code and were not the bug.

## Debug session notes (PC13 blink)

- What was tried first: reviewed `HAL_Init()`/`SystemClock_Config()` ordering, GPIO clock enable ordering, and active-low pin logic — all were already correct, so none of these were the actual bug.
- What failed: build and flash both reported SUCCESS and the chip reset, but PC13 never blinked.
- Root cause found by inspecting `startup_stm32f103xb.s` in the PlatformIO package cache (`~/.platformio/packages/framework-stm32cubef1/.../gcc/startup_stm32f103xb.s`): `SysTick_Handler` was weak-aliased to `Default_Handler`, an infinite loop, because the project had no `stm32f1xx_it.c` defining it.
- What worked: added a `SysTick_Handler` calling `HAL_IncTick()` directly in `src/main.c`. Rebuilt with `pio run` → SUCCESS. LED now expected to blink every 250ms.

## Known Display Driver Gotchas — PCD8544 Nokia 5110

1. **Blank display is almost always a wrong Vop contrast value, not wiring or SPI failure.** A wrong `Vop` doesn't fail loudly — the controller still accepts every command and data byte correctly, it just renders outside the visible contrast window, so the glass looks dead.
   - Correct init values for this panel: `Vop=0xB8`, temp coeff=`0x04`, bias=`0x14` (not the commonly-cited `0xC0`/`0x06`/`0x13`).
   - RST must be held LOW for 100ms minimum, not 10ms.
   - Diagnostic clue: a brief flicker during MCU reset or during power-off means SPI/wiring is actually working — DDRAM is being written correctly the whole time — and only the contrast value is wrong. The supply voltage briefly sweeps through the panel's real usable contrast range on its way up/down, revealing content that's otherwise invisible at a stable 3.3V. Don't chase wiring/SPI bugs on this symptom — sweep `Vop` (the 7-bit value in the `0x80 | Vop` "Set Vop" extended command) across its full range instead.
   ```c
   HAL_GPIO_WritePin(RST_PORT, RST_PIN, GPIO_PIN_RESET);
   HAL_Delay(100); /* not 10ms */
   HAL_GPIO_WritePin(RST_PORT, RST_PIN, GPIO_PIN_SET);

   pcd8544_send_cmd(0x21); /* extended instruction set */
   pcd8544_send_cmd(0xB8); /* Vop */
   pcd8544_send_cmd(0x04); /* temperature coefficient */
   pcd8544_send_cmd(0x14); /* bias 1:48 */
   pcd8544_send_cmd(0x20); /* back to basic instruction set */
   pcd8544_send_cmd(0x0C); /* normal display mode */
   ```
2. **SPI configuration**: Mode 0 (`CPOL=0`, `CPHA=0`), MSB first, 8-bit data size, max 4MHz (the PCD8544 datasheet's hard cap — do not clock it faster).
3. **Test pattern warning**: alternating `0x55`/`0xAA` per byte produces a 1-pixel checkerboard — since each DDRAM byte maps to 8 vertically stacked pixels in one column, this is the finest pattern physically possible on the display, and it looks blank/uniform to the naked eye even when the driver and wiring are working correctly. Always use 8×8 pixel blocks or a full `0xFF` fill to visually confirm the display is working:
   ```c
   const int block = 8;
   for (int page = 0; page < PCD8544_HEIGHT / 8; page++) {
       pcd8544_send_cmd(0x80);        /* column 0 */
       pcd8544_send_cmd(0x40 | page); /* select this page */
       for (int col = 0; col < PCD8544_WIDTH; col++) {
           int block_x = col / block;
           pcd8544_send_data(((block_x + page) % 2) ? 0xFF : 0x00);
       }
   }
   ```
4. **CE pin must go LOW before each byte and HIGH after — do not leave CE asserted continuously.** Toggle it around every single `HAL_SPI_Transmit` call (one byte at a time), not once for a whole burst of bytes.

## Debug session notes (PCD8544 blank display)

- Symptom: display completely blank after flashing, despite correct SPI config (Mode 0, MSB-first, 8-bit, clock within the 1–4MHz spec) and correct CE/DC toggling around every byte.
- Red herrings ruled out one at a time: SPI clock speed (was 9MHz, lowered to 2.25MHz — no change), a full contrast sweep across the entire Vop range while the MCU's own heartbeat LED confirmed the firmware was alive and running (no change), reseating every jumper wire (no change).
- Key clue that cracked it: a brief blink was visible during the MCU reset transient, and a stripe was visible during power-off. Both are consistent with DDRAM already holding correct data the whole time, with contrast simply set outside the visible range at steady-state 3.3V — voltage passing through the panel's usable window during power transients is what made it briefly visible.
- Fix: changed `Vop` from `0xC0` to `0xB8`, temp coefficient from `0x06` to `0x04`, bias from `0x13` to `0x14`, and held RST low for 100ms instead of 10ms. Confirmed with a solid `0xFF` fill (whole glass went solid black) before moving to any pixel-level pattern.
- Second, unrelated issue found immediately after: the checkerboard test pattern itself (`0x55`/`0xAA` per byte, a 1-pixel checkerboard) was indistinguishable from blank at this display's pixel pitch. Switched to 8×8 pixel block squares, which is clearly visible.

## Live Watch (Cortex-Debug) gotchas

Full debug log: [LIVE_WATCH.md](LIVE_WATCH.md). Short version:

1. `.vscode/launch.json` is regenerated by PlatformIO — any extra key added to the `platformio-debug` configs (e.g. `liveWatch`) gets silently stripped, and that debug adapter doesn't support Live Watch anyway. Use a separate `"type": "cortex-debug"` config instead (requires the `marus25.cortex-debug` extension); PlatformIO's regenerator never touches non-`platformio-debug` entries.
2. After installing `marus25.cortex-debug` via CLI, reload the VS Code window before it will work.
3. This machine has two ARM toolchains installed: `toolchain-gccarmnoneeabi@1.70201.0` (GDB 8, used for the build) and unversioned `toolchain-gccarmnoneeabi` (GDB 13). Live Watch requires GDB ≥ 9 — point the debug config's `gdbPath`/`armToolchainPath` at the unversioned one.
4. `platformio.ini` needs `build_type = debug` or the build has no DWARF type info and GDB can't read variables properly.
5. Watch `blink_counter` in the **Cortex Live Watch** panel (collapsed near the bottom of Run & Debug), not the plain **Watch** panel — plain Watch only evaluates while halted and always shows "not available" while the target runs freely.
