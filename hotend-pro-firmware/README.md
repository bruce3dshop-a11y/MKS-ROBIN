# HOTEND PRO — Smart PID Controller Firmware

**Board:** MKS Robin Nano V3.1 (STM32F407VET6)  
**Display:** LCD12864 Smart Controller (ST7920, Software SPI)  
**Framework:** STM32 HAL — no Arduino  
**Version:** 1.0

---

## Features

| Feature | Detail |
|---|---|
| PID control | Soft-start, integral anti-windup, derivative-on-measurement |
| Autotune | Åström-Hägglund relay-feedback → Ziegler-Nichols coefficients |
| Display | 128×64 ST7920, double-buffered, 20 FPS |
| Boot screen | Fade-in logo → 2 s hold → fade-out |
| Home screen | Large temp, SET target, power bar, ETA, 60-second graph |
| Flame animation | 3-frame animated heating indicator |
| Settings menu | Max temp, beeper toggle, factory reset, save |
| Statistics | Runtime, heating cycles, max temp, average PWM, total heat time |
| Lock mode | 3-second hold to lock/unlock all inputs |
| Flash storage | PID coefficients, settings, statistics, last target — Sector 11 |
| Safety | Thermal runaway, open/short thermistor, over-temperature, watchdog |
| Sounds | Short beep, double beep, long beep |

---

## Pin Assignments

Verify against your hardware before flashing.

| Signal | Port/Pin | Connector |
|---|---|---|
| LCD CS | PB6 | EXP2 |
| LCD SCK | PA5 | EXP2 |
| LCD MOSI/SID | PA7 | EXP2 |
| Encoder A | PC6 | EXP2 BTN_EN1 |
| Encoder B | PC7 | EXP2 BTN_EN2 |
| Encoder Button | PB1 | EXP1 BTN_ENC |
| Beeper | PC5 | EXP1 |
| Heater HE0 | PE5 | HE0 FET |
| Thermistor TH0 | PC1 | TH0 ADC |

To change pins, edit **`include/config.h`** — all pin definitions are in one place.

---

## Project Structure

```
hotend-pro-firmware/
├── platformio.ini          # Build configuration
├── linker/
│   └── STM32F407VETx_APP.ld  # Linker script (app at 0x08010000)
├── include/
│   ├── config.h            # Hardware pins, constants, limits
│   └── stm32f4xx_hal_conf.h
├── scripts/
│   └── generate_bin.py     # Post-build: output Robin_nano_v3.1.bin
├── src/
│   ├── main.cpp            # Entry point, clock config, main loop
│   ├── boot/               # Animated boot screen
│   ├── display/            # ST7920 software SPI driver
│   ├── graphics/           # Framebuffer, drawing primitives, fonts
│   ├── encoder/            # Quadrature decode + button events
│   ├── temperature/        # ADC + NTC Steinhart-Hart thermistor
│   ├── heater/             # TIM9 PWM output on PE5
│   ├── pid/                # PID controller + autotune
│   ├── settings/           # Settings struct management
│   ├── storage/            # Flash read/write (Sector 11)
│   └── ui/                 # All screens and input routing
└── README.md
```

---

## Build Instructions

### Prerequisites

1. Install [PlatformIO](https://platformio.org/install/cli)
2. The `ststm32` platform and `stm32cube` framework install automatically

### Compile

```bash
cd hotend-pro-firmware
pio run
```

The output binary is placed at:

```
Robin_nano_v3.1.bin
```

### Flash via SD Card (MKS bootloader)

1. Format a microSD card as FAT32
2. Copy `Robin_nano_v3.1.bin` to the root of the SD card
3. Insert the SD card into the MKS Robin Nano V3.1
4. Power on — the bootloader detects the file and flashes it automatically
5. The board reboots into the new firmware

> **Note:** The linker script places the application at `0x08010000` (after the 64 KB MKS bootloader). Do **not** flash via ST-Link directly to `0x08000000` unless you have replaced the bootloader.

### Flash via ST-Link (clean board, no bootloader)

Change `linker/STM32F407VETx_APP.ld` flash origin to `0x08000000` and remove `VECT_TAB_OFFSET` from `platformio.ini`, then:

```bash
pio run --target upload --upload-port /dev/ttyACM0
```

---

## Encoder Operation

| Action | Effect |
|---|---|
| Rotate CW | Increase target temperature (+1°C) |
| Rotate CCW | Decrease target temperature (−1°C) |
| Short click | Confirm and save target temperature |
| Hold 3 seconds | Lock / unlock controller |

**From Home screen, long-hold with target = 0** → enter Settings menu.

### Settings Menu Navigation

- Rotate to scroll items
- Click to enter edit / execute action
- Hold to exit to Home

### Screens

| Screen | Access |
|---|---|
| Home | Default |
| Settings | Long-hold from Home (target=0) |
| PID Autotune | Select from Settings |
| Statistics | Select from Settings |
| Lock | Long-hold from Home |

---

## PID Autotune

1. Set a target temperature from the Home screen (200°C recommended)
2. Enter Settings → PID Autotune
3. Press the encoder to start
4. The controller runs 8 relay cycles and calculates Kp, Ki, Kd
5. Coefficients are saved to flash automatically
6. The process takes 3–10 minutes depending on thermal mass

---

## Safety Features

| Feature | Behaviour |
|---|---|
| Open thermistor | ADC < 100 → immediate heater shutdown |
| Shorted thermistor | ADC > 4090 → immediate heater shutdown |
| Over-temperature | temp > 300°C → heater off (no PID fault) |
| Thermal runaway | If temp rises < 2°C in 30 s while heating → fault + shutdown |
| Watchdog | STM32 IWDG, 2-second timeout; HardFault also kills heater |

After a fault, the display shows ERROR. Power-cycle to reset (fault is cleared on boot).

---

## Flash Storage Layout

| Address | Size | Content |
|---|---|---|
| `0x08000000` | 64 KB | MKS bootloader |
| `0x08010000` | 448 KB | Application |
| `0x080E0000` | 128 KB | Settings / PID / Statistics (Sector 11) |

---

## Tuning

Default PID values (Kp=22.2, Ki=1.08, Kd=114.0) are reasonable starting points for a typical 40W E3D V6-style hotend. Run autotune for your specific heater/thermistor combination.

The thermistor model uses the Beta equation with:

- R₀ = 100 kΩ at 25°C  
- B = 4092 K  
- Series resistor = 4.7 kΩ  

Adjust `THERMISTOR_B_COEFF` and `THERMISTOR_SERIES_R` in `config.h` for other thermistors.

---

## License

MIT — free to use, modify, and distribute.
