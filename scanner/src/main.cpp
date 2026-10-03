#include <Arduino.h>
#include <WiFi.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

#define WIFI_SCAN_INTERVAL_MS  8000
#define BLE_SCAN_DURATION_S    5

static const char* encryptionName(wifi_auth_mode_t mode) {
    switch (mode) {
        case WIFI_AUTH_OPEN:          return "OPEN";
        case WIFI_AUTH_WEP:           return "WEP";
        case WIFI_AUTH_WPA_PSK:       return "WPA";
        case WIFI_AUTH_WPA2_PSK:      return "WPA2";
        case WIFI_AUTH_WPA_WPA2_PSK:  return "WPA/2";
        case WIFI_AUTH_WPA3_PSK:      return "WPA3";
        default:                      return "????";
    }
}

static void scanWifi() {
    Serial.println("\n╔══════════════════════════════════════ WIFI ══════════════════════════════════════╗");
    int n = WiFi.scanNetworks(false, true);  // async=false, show_hidden=true
    if (n == 0) {
        Serial.println("  (no se encontraron redes)");
    } else {
        Serial.printf("  %-32s %-18s CH  RSSI    SEGURIDAD\n", "SSID", "BSSID");
        Serial.println("  ────────────────────────────────────────────────────────────────────────────────");
        for (int i = 0; i < n; i++) {
            String ssid = WiFi.SSID(i);
            if (ssid.length() == 0) ssid = "(oculta)";
            Serial.printf("  %-32s %-18s %2d  %4d dBm  %s\n",
                ssid.substring(0, 32).c_str(),
                WiFi.BSSIDstr(i).c_str(),
                WiFi.channel(i),
                WiFi.RSSI(i),
                encryptionName(WiFi.encryptionType(i)));
        }
    }
    Serial.println("╚══════════════════════════════════════════════════════════════════════════════════╝");
    WiFi.scanDelete();
}

class BleCallback : public BLEAdvertisedDeviceCallbacks {
public:
    int count = 0;

    void onResult(BLEAdvertisedDevice dev) override {
        count++;
        String name = dev.haveName() ? dev.getName().c_str() : "(sin nombre)";
        Serial.printf("  %-32s  %s  %4d dBm",
            name.substring(0, 32).c_str(),
            dev.getAddress().toString().c_str(),
            dev.getRSSI());
        if (dev.haveManufacturerData()) {
            std::string mfr = dev.getManufacturerData();
            if (mfr.size() >= 2) {
                uint16_t cid = (uint8_t)mfr[1] << 8 | (uint8_t)mfr[0];
                Serial.printf("  CID=0x%04X", cid);
            }
        }
        Serial.println();
    }
};

static void scanBle() {
    Serial.println("\n╔══════════════════════════════════════ BLE ═══════════════════════════════════════╗");
    Serial.printf("  %-32s  %-17s  RSSI\n", "NOMBRE", "MAC");
    Serial.println("  ────────────────────────────────────────────────────────────────────────────────");

    BLEScan* scan = BLEDevice::getScan();
    BleCallback cb;
    scan->setAdvertisedDeviceCallbacks(&cb, false);
    scan->setActiveScan(false);
    scan->setInterval(100);
    scan->setWindow(99);
    scan->start(BLE_SCAN_DURATION_S, false);
    scan->clearResults();

    if (cb.count == 0) Serial.println("  (no se encontraron dispositivos)");
    Serial.println("╚══════════════════════════════════════════════════════════════════════════════════╝");
}

void setup() {
    // Apagar LEDs de fábrica
    neopixelWrite(48, 0, 0, 0);   // RGB WS2812 → off
    pinMode(2, OUTPUT);
    digitalWrite(2, LOW);          // LED azul → off

    Serial.begin(115200);
    delay(1000);
    Serial.println("\n\n=== ESP32-S3 WiFi + BLE Scanner ===");

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    BLEDevice::init("");
}

void loop() {
    scanWifi();
    scanBle();
    Serial.printf("\n[esperando %d s...]\n", WIFI_SCAN_INTERVAL_MS / 1000);
    delay(WIFI_SCAN_INTERVAL_MS);
}
