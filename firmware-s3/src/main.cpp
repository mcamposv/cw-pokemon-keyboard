#include <Arduino.h>
#include "config.h"
#include "version.h"
#include "ps2.hpp"
#include "ble_hid.hpp"
#include "usb_kbd.hpp"

// ── LED helpers ───────────────────────────────────────────────────────────────

static void led(uint8_t r, uint8_t g, uint8_t b) {
    neopixelWrite(PIN_LED_RGB, r, g, b);
}

static void led_task(void* arg) {
    while (true) {
        if (usb_kbd_is_active()) {
            // USB keyboard connected: LED off (physical keyboard = no visual noise)
            led(0, 0, 0);
            vTaskDelay(pdMS_TO_TICKS(500));
        } else if (ble_hid_is_connected()) {
            // BLE connected: slow blue blink
            led(0, 0, 20);
            vTaskDelay(pdMS_TO_TICKS(800));
            led(0, 0, 0);
            vTaskDelay(pdMS_TO_TICKS(800));
        } else {
            // Scanning: fast blue blink
            led(0, 0, 30);
            vTaskDelay(pdMS_TO_TICKS(120));
            led(0, 0, 0);
            vTaskDelay(pdMS_TO_TICKS(120));
        }
    }
}

// ── Setup & loop ──────────────────────────────────────────────────────────────

void setup() {
    led(0, 0, 0);   // kill factory LED immediately
    pinMode(2, OUTPUT);
    digitalWrite(2, LOW);

    Serial.begin(115200);
    delay(500);
    Serial.println("\n=== CWPokeKey v" FIRMWARE_VERSION " ===");
    Serial.println("USB+BLE HID → PS/2 adapter for CW Pokemon");

    ps2_init();
    usb_kbd_init();
    // ble_hid_init();  // BLE disabled for USB-only test

    xTaskCreatePinnedToCore(led_task, "led", 2048, nullptr, 1, nullptr, 1);
}

void loop() {
    // Everything runs in tasks; loop just keeps Arduino runtime happy
    vTaskDelay(pdMS_TO_TICKS(1000));
}
