#include "ps2.hpp"
#include "config.h"
#include "driver/gpio.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <Arduino.h>

// Push-pull: conducimos activamente HIGH (3.3V) y LOW (0V).
// El CW Pokemon tiene pull-down en DATA — open-drain no puede ganarle.
// CLK: CW Pokemon tiene pull-up a 3.2V, push-pull también funciona.

static portMUX_TYPE      ps2_mux     = portMUX_INITIALIZER_UNLOCKED;
static SemaphoreHandle_t ps2_bus_sem = nullptr;

// ── Envío de un byte ──────────────────────────────────────────────────────────

static void ps2_tx_byte(uint8_t byte) {
    uint8_t  parity = !__builtin_parity(byte);
    // frame: bit 0 = start(0), bits 1-8 = datos LSB first, bit 9 = paridad, bit 10 = stop(1)
    uint16_t frame  = ((uint16_t)byte   << 1) |
                      ((uint16_t)parity << 9) |
                      ((uint16_t)1      << 10);

    portENTER_CRITICAL(&ps2_mux);
    for (int i = 0; i < 11; i++) {
        gpio_set_level(PIN_PS2_DATA, (frame >> i) & 1);
        esp_rom_delay_us(5);
        gpio_set_level(PIN_PS2_CLK, 0);          // flanco descendente → host muestrea
        esp_rom_delay_us(PS2_HALF_US);
        gpio_set_level(PIN_PS2_CLK, 1);
        esp_rom_delay_us(PS2_HALF_US - 5);
    }
    portEXIT_CRITICAL(&ps2_mux);

    gpio_set_level(PIN_PS2_DATA, 1);              // línea idle
}

// ── Tarea: envía BAT al arranque ──────────────────────────────────────────────

static void ps2_bat_task(void*) {
    vTaskDelay(pdMS_TO_TICKS(500));
    xSemaphoreTake(ps2_bus_sem, portMAX_DELAY);
    Serial.println("[PS2] → BAT 0xAA");
    ps2_tx_byte(0xAA);
    xSemaphoreGive(ps2_bus_sem);
    vTaskDelete(nullptr);  // tarea de un solo uso
}

// ── API pública ───────────────────────────────────────────────────────────────

void ps2_init() {
    // Push-pull: conducimos activamente ambas líneas
    gpio_config_t cfg = {};
    cfg.pin_bit_mask = (1ULL << PIN_PS2_CLK) | (1ULL << PIN_PS2_DATA);
    cfg.mode         = GPIO_MODE_OUTPUT;
    cfg.pull_up_en   = GPIO_PULLUP_DISABLE;
    cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    cfg.intr_type    = GPIO_INTR_DISABLE;
    gpio_config(&cfg);

    gpio_set_level(PIN_PS2_CLK,  1);
    gpio_set_level(PIN_PS2_DATA, 1);

    ps2_bus_sem = xSemaphoreCreateBinary();
    xSemaphoreGive(ps2_bus_sem);

    xTaskCreatePinnedToCore(ps2_bat_task, "ps2_bat", 2048, nullptr, 3, nullptr, 1);
}

void ps2_key_down(uint16_t ps2_code) {
    if (!ps2_code) return;
    if (ps2_code & 0x0100)
        Serial.printf("[PS2] TX E0 %02X\n", ps2_code & 0xFF);
    else
        Serial.printf("[PS2] TX %02X\n", ps2_code & 0xFF);
    xSemaphoreTake(ps2_bus_sem, portMAX_DELAY);
    if (ps2_code & 0x0100) ps2_tx_byte(0xE0);
    ps2_tx_byte(ps2_code & 0xFF);
    xSemaphoreGive(ps2_bus_sem);
}

void ps2_key_up(uint16_t ps2_code) {
    if (!ps2_code) return;
    if (ps2_code & 0x0100)
        Serial.printf("[PS2] TX E0 F0 %02X\n", ps2_code & 0xFF);
    else
        Serial.printf("[PS2] TX F0 %02X\n", ps2_code & 0xFF);
    xSemaphoreTake(ps2_bus_sem, portMAX_DELAY);
    if (ps2_code & 0x0100) ps2_tx_byte(0xE0);
    ps2_tx_byte(0xF0);
    ps2_tx_byte(ps2_code & 0xFF);
    xSemaphoreGive(ps2_bus_sem);
}
