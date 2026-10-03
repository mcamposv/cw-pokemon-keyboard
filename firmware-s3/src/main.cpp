#include <Arduino.h>
#include "config.h"
#include "version.h"
#include "ps2.hpp"
#include "usb_kbd.hpp"

// ── LED helpers ───────────────────────────────────────────────────────────────

static void led(uint8_t r, uint8_t g, uint8_t b) {
    neopixelWrite(PIN_LED_RGB, r, g, b);
}

static void led_task(void* arg) {
    while (true) {
        if (usb_kbd_is_active()) {
            led(0, 0, 0);
            vTaskDelay(pdMS_TO_TICKS(500));
        } else {
            // Waiting for keyboard: fast blue blink
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
    Serial.println("USB HID → PS/2 adapter for CW Pokemon");

    ps2_init();
    usb_kbd_init();

    xTaskCreatePinnedToCore(led_task, "led", 2048, nullptr, 1, nullptr, 1);
}

void loop() {
    // Everything runs in tasks; loop just keeps Arduino runtime happy
    vTaskDelay(pdMS_TO_TICKS(1000));
}
