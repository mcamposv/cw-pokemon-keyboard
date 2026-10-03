#pragma once
#include "driver/gpio.h"

// ── PS/2 output pins ──────────────────────────────────────────────────────────
#define PIN_PS2_CLK   GPIO_NUM_6   // TRS RING
#define PIN_PS2_DATA  GPIO_NUM_5   // TRS TIP

// PS/2 timing — 40 μs half-period ≈ 12.5 kHz, well within PS/2 spec (60–100 μs period)
#define PS2_HALF_US   40

// ── Status LED ────────────────────────────────────────────────────────────────
#define PIN_LED_RGB   48           // WS2812 on DevKitC-1

// ── BLE ───────────────────────────────────────────────────────────────────────
#define BLE_RECONNECT_TIMEOUT_S  60   // scan for new device after this many seconds
#define BLE_DEVICE_NAME          "CWPokeKey"

// ── NVS namespace ────────────────────────────────────────────────────────────
#define NVS_NAMESPACE  "cwkey"
#define NVS_KEY_ADDR   "ble_addr"
