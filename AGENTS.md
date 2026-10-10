# KeyBard9001

Custom 1800-layout keyboard firmware for an STM32F407VGT6. The tree is a STM32CubeMX CMake project. Code we write lives in `00_DesignFiles/App/`. Hardware intent lives in `01_Docs/`.

## Layout

- `00_DesignFiles/` is the firmware. Open this directory for configure and build. The CubeMX project is `STM32F407VGT6.ioc`.
- `00_DesignFiles/App/` is ours. Helper libraries and other firmware we write go here, kept separate from CubeMX output. List each new source and the `App` include path in the user sections of `00_DesignFiles/CMakeLists.txt`.
- `00_DesignFiles/CMakeLists.txt` is generated once. Add new sources, includes, and libraries in its marked user sections.
- `00_DesignFiles/cmake/stm32cubemx/` is regenerated from the `.ioc`. Do not hand-edit it.
- `Core/`, `USB_DEVICE/`, `Drivers/`, `Middlewares/`, `startup_stm32f407xx.s`, and `STM32F407xx_FLASH.ld` are CubeMX output.
- `01_Docs/` holds design notes, the BOM, the schematic PDF, the marked-up layout, and required board mods.

## Hardware the firmware must match

- MCU: STM32F407VGT6, LQFP100, 8 MHz HSE. SYSCLK is 72 MHz (`PLLM=4`, `PLLN=72`, `PLLP=2`). USB is 48 MHz (`PLLQ=3`). Leave this clock alone unless TIM1, TIM6, and USB are retuned with it.
- USB HID full-speed on PA11/PA12. Product string `Keybard9001`, manufacturer `TheeKraft`, VID `1155`, PID `22315`.
- Matrix: rows `R1`–`R6` are outputs, idle high. Columns `C1`–`C19` are inputs. Pin macros are in `Core/Inc/main.h`.
- SK6812MINI-E LEDs: TIM1 CH1 PWM on PA8, DMA2 Stream1, memory to peripheral, halfword. ARR is 89, so the bit clock is 800 kHz at 72 MHz. Do not retune this timer without matching the LED protocol. Sending a 0: CCR 22: High for (0.306us), Low for (0.944us)
Sending a 1: CCR 65: High for (0.903us), Low for (0.347us)
- TIM6 is a 1 kHz tick (prescaler 71, period 999 on a 72 MHz timer clock). This timer is for the keyboard key scan polling
- I2C1 (PB6/PB7): SSD1306 OLED at `0x3C`, and the M24C02 EEPROM from the BOM.
- USART1 (PA9/PA10) is debug. USART2 (PA2/PA3) is the expansion header.
- Encoder on PC1 (`ENC_BTN`), PC2 (`ENC_B`), PC3 (`ENC_A`). Extra button `BTN1` on PD0.
- ADC1 converts PA5 (channel 5). PA4, PA6, and PA7 are also analog pins.

## Generated code

Never change pin, clock, or peripheral configuration that CubeMX owns. That includes `MX_*_Init` bodies, GPIO setup, the clock tree, DMA and NVIC setup, pin macros in `Core/Inc/main.h`, hand edits to `STM32F407VGT6.ioc`, and `cmake/stm32cubemx/`. Make those settings in STM32CubeMX, then run Generate Code into this repo. The next generate overwrites anything outside the user markers.

In Cube-generated `.c` and `.h` files, add code only between `USER CODE BEGIN` and `USER CODE END`. Put helper libraries and other new firmware in `00_DesignFiles/App/`, and call them from the user blocks in `Core/Src/main.c`. Do not place our code under `Core/`, `USB_DEVICE/`, `Drivers/`, or `Middlewares/`. Do not edit `Drivers/` or `Middlewares/` to change application behavior.

## Build

Needs CMake 3.22+, Ninja, and `arm-none-eabi-gcc` on `PATH`. From `00_DesignFiles`:

```
cmake --preset Debug
cmake --build --preset Debug
```

`Release` is the other preset. The image is `00_DesignFiles/build/Debug/STM32F407VGT6.elf`. There is no host test suite. A successful build is the check available in this repo; behavior past that needs the board.
