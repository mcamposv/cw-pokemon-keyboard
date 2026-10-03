# CWPokeKey — USB keyboard adapter for CW Pokemon

Firmware for ESP32-S3 that converts input from a standard wired USB keyboard to PS/2 protocol, enabling its use with the 3.5mm TRS PS/2 jack of the [CW Pokemon](https://github.com/admvip/CW-Pokemon-Infomation).

## Goal

The [CW Pokemon](https://github.com/admvip/CW-Pokemon-Infomation) is a hardware device for amateur radio operators that decodes and generates Morse code (CW). It only accepts PS/2 keyboards through its 3.5mm TRS jack. This adapter bridges that gap using an ESP32-S3 as USB HID host and PS/2 bit-bang transmitter.

```
USB Keyboard  →  ESP32-S3 (USB OTG Host)  →  PS/2 bit-bang  →  3.5mm TRS jack  →  CW Pokemon
```

**CW Pokemon project (original hardware):** [github.com/admvip/CW-Pokemon-Infomation](https://github.com/admvip/CW-Pokemon-Infomation)

## Bill of materials

| Qty | Component | Notes |
|---|---|---|
| 1 | ESP32-S3-DevKitC-1 N16R8 | 16 MB flash, 8 MB PSRAM |
| 1 | Female stereo TRS 3.5mm pigtail (for soldering) | **Must be stereo** — see warning below |
| 1 | Stereo TRS 3.5mm cable (male-to-male) | Connects the adapter to the CW Pokemon |
| 1 | USB-C to USB-A OTG cable (male-to-female) | Connects the keyboard to the ESP32 OTG port |
| 2 | Fine wire (~28 AWG, ~5 cm each) | 5V and GND to power the keyboard — see mod below |
| 1 | USB-C cable | Powers the ESP32 (right/UART port) |
| 1 | 5V USB power adapter | To power the ESP32 |
| — | Solder + soldering iron | PCB soldering |
| — | Hot glue gun | Secure and protect solder joints |

> **Warning:** The TRS jack must be **stereo** (3 contacts: TIP, RING, SLEEVE). A mono (TS, 2-contact) connector would short DATA to GND and could damage the CW Pokemon.

## TRS jack wiring (PS/2)

| TRS contact | PS/2 signal | ESP32-S3 GPIO |
|---|---|---|
| TIP | DATA | GPIO5 |
| RING | CLK | GPIO6 |
| SLEEVE | GND | GND |

**Enable keyboard mode on the CW Pokemon:** Menu → tipo de electrónico → **KeyB**

## Hardware modification — VBUS power for the keyboard

The USB-C to USB-A OTG cable handles the data lines (D+/D−) between the ESP32-S3 OTG port and the keyboard. However, the USB keyboard also needs power (5V VBUS), which the DevKitC-1's OTG port does not supply on its own.

The fix is to solder **2 short wires** directly on the board:

| Pad to solder | Wire | Destination |
|---|---|---|
| VBUS pad of the USB-OTG connector (left) | Red (5V) | 5V pin on the DevKitC-1 header |
| GND pad of the USB-OTG connector (left) | Black (GND) | GND pin on the DevKitC-1 header |

The exact pads are marked with **red circles** in the reference photos below.

The wires are secured with hot glue to prevent the solder joints from lifting when plugging/unplugging the OTG cable.

### Hardware photos

| File | Description |
|---|---|
| `s3Pictures/photo_2026-10-03_12-48-56.jpg` | **Pads marked with red circles** — exact points to solder the 2 power wires |
| `s3Pictures/photo_2026-10-03_12-49-09.jpg` | Back view: soldered wires and hot glue over the OTG connector |
| `s3Pictures/photo_2026-10-03_12-49-05.jpg` | Overview of the ESP32-S3 N16R8 module with both USB-C connectors |
| `s3Pictures/photo_2026-10-03_12-49-00.jpg` | Side view with the OTG cable connected |
| `s3Pictures/photo_2026-10-03_12-48-09.jpg` | Close-up of the USB-OTG connector with hot glue |
| `s3Pictures/photo_2026-09-26_18-58-44.jpg` | Additional detail of the solder joints |

## Important electrical notes

- The CW Pokemon uses **3.3V logic** — no level shifter is needed.
- The **CLK (RING)** pin has an internal pull-up to **3.2V** on the CW Pokemon.
- The **DATA (TIP)** pin has an internal **~8 kΩ pull-down to GND** on the CW Pokemon.
- **CRITICAL — push-pull mode for DATA:** With the ESP32's internal 45 kΩ pull-up in open-drain mode, the DATA line only reaches ~0.5V because the CW Pokemon's 8 kΩ pull-down wins. The CW Pokemon reads DATA as constant LOW and all PS/2 data arrives corrupted. By configuring the GPIO in **push-pull** (active output) mode, the ESP32 actively drives 3.3V and overcomes the pull-down. Implemented in `firmware-s3/src/ps2.cpp`.

## LED indicator (WS2812 RGB, GPIO48)

| State | LED behavior |
|---|---|
| USB keyboard connected | Off |
| BLE connected | Blue, slow blink (~800 ms) |
| Scanning / waiting | Blue, fast blink (~120 ms) |

## Build and flash

Requires [PlatformIO](https://platformio.org/) (VS Code extension or CLI).

```bash
cd firmware-s3
pio run              # build
pio run -t upload    # flash (port /dev/ttyACM0)
pio device monitor   # serial monitor (115200 baud)
```

### Keyboard layout selection

Edit `firmware-s3/platformio.ini`:

```ini
build_flags =
    -DLAYOUT_ES   ; Spanish QWERTY (default)
;   -DLAYOUT_US   ; US QWERTY
```

## Project structure

```
firmware-s3/              — Main firmware (USB HID Host + PS/2 bit-bang + BLE)
├── platformio.ini
└── src/
    ├── config.h          — Pin assignments and constants
    ├── ps2.cpp/hpp       — PS/2 bit-bang driver (push-pull mode)
    ├── usb_kbd.cpp/hpp   — USB HID Host (ESP-IDF usb_host)
    ├── ble_hid.cpp/hpp   — BLE HID Host (NimBLE) — in development
    ├── keymap.c/h        — HID keycode → PS/2 Set 2 scancode table
    └── main.cpp
scanner/                  — Diagnostic tool: WiFi + BLE scanner
s3Pictures/               — Hardware and soldering photos
debug/                    — Debug screenshots
```

## Repository branches

| Branch | Contents |
|---|---|
| `usb-bt` | Full USB + BLE firmware (BLE in development), diagnostic scanner tool |
| `only-usb` | USB→PS/2 firmware only, no BLE code or extra tools |

## PS/2 timing

- CLK half-period: 40 μs → ~12.5 kHz (PS/2 spec: 60–100 μs period, i.e. ≤ 16.7 kHz)
- Verified with oscilloscope: 928 μs for 11 bits (one full byte)
- The CW Pokemon resets its PS/2 state machine if there are more than ~2 ms between CLK edges during a byte transfer — at 40 μs half-periods we are well within that limit

## License

GNU General Public License v3.0

Copyright (C) 2026 mcamposv
