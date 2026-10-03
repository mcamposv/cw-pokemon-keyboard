#include "ble_hid.hpp"
#include "config.h"
#include "keymap.h"
#include "ps2.hpp"
#include "usb_kbd.hpp"

#include <NimBLEDevice.h>
#include <Preferences.h>
#include <Arduino.h>
#include <map>
#include <string>

// ── HID report parsing ───────────────────────────────────────────────────────

struct HidReport {
    uint8_t modifier;
    uint8_t keys[6];
};

static HidReport s_prev = {};

static void process_report(const HidReport& r) {
    // Modifiers: compare bit by bit
    uint8_t changed = r.modifier ^ s_prev.modifier;
    for (int i = 0; i < 8; i++) {
        if (!(changed & (1 << i))) continue;
        uint16_t code = MODIFIER_TO_PS2[i];
        if (r.modifier & (1 << i)) ps2_key_down(code);
        else                        ps2_key_up(code);
    }

    // Keys released: in prev but not in current
    for (int i = 0; i < 6; i++) {
        if (!s_prev.keys[i]) continue;
        bool still_held = false;
        for (int j = 0; j < 6; j++)
            if (r.keys[j] == s_prev.keys[i]) { still_held = true; break; }
        if (!still_held) {
            uint16_t code = HID_TO_PS2[s_prev.keys[i]];
            if (code) ps2_key_up(code);
        }
    }

    // Keys pressed: in current but not in prev
    for (int i = 0; i < 6; i++) {
        if (!r.keys[i]) continue;
        bool was_held = false;
        for (int j = 0; j < 6; j++)
            if (s_prev.keys[j] == r.keys[i]) { was_held = true; break; }
        if (!was_held) {
            uint16_t code = HID_TO_PS2[r.keys[i]];
            if (code) ps2_key_down(code);
        }
    }

    s_prev = r;
}

static void parse_hid_data(const uint8_t* data, size_t len) {
    HidReport r = {};
    if (len >= 8) {
        // Standard boot-protocol format: [mod][reserved][k0..k5]
        r.modifier = data[0];
        for (int i = 0; i < 6 && (i + 2) < (int)len; i++)
            r.keys[i] = data[i + 2];
    } else if (len >= 3) {
        // Some keyboards prefix with report ID — try to detect
        r.modifier = data[1];
        for (int i = 0; i < 6 && (i + 3) < (int)len; i++)
            r.keys[i] = data[i + 3];
    } else {
        return;
    }
    process_report(r);
}

// ── NimBLE callbacks ─────────────────────────────────────────────────────────

static volatile bool s_connected = false;
static NimBLEClient* s_client = nullptr;

// Queue item: address string + type byte, e.g. "d3:d8:e4:87:79:78|1"
struct AddrItem { char buf[42]; };
static QueueHandle_t s_addr_queue = nullptr;

// Cooldown: don't retry a failed address for 120s
static std::map<std::string, uint32_t> s_fail_time;
static bool addr_on_cooldown(const char* addr) {
    auto it = s_fail_time.find(addr);
    if (it == s_fail_time.end()) return false;
    return (millis() - it->second) < 30000;
}
static void mark_fail(const char* addr) {
    s_fail_time[addr] = millis();
}

static void notify_cb(NimBLERemoteCharacteristic*, uint8_t* data, size_t len, bool) {
    if (usb_kbd_is_active()) return;  // USB keyboard has priority
    parse_hid_data(data, len);
}

struct ClientCB : public NimBLEClientCallbacks {
    void onConnect(NimBLEClient*) override {
        s_connected = true;
        Serial.println("[BLE] connected");
    }
    void onDisconnect(NimBLEClient*) override {
        s_connected = false;
        s_prev = {};  // release all keys
        Serial.println("[BLE] disconnected");
    }
};
static ClientCB s_client_cb;

struct SecCB : public NimBLESecurityCallbacks {
    bool onSecurityRequest() override {
        Serial.println("[SEC] security request → accept");
        return true;
    }
    uint32_t onPassKeyRequest() override {
        Serial.println("[SEC] passkey request → 0");
        return 0;
    }
    bool onConfirmPIN(uint32_t pin) override {
        Serial.printf("[SEC] confirm PIN %06lu → yes\n", (unsigned long)pin);
        return true;
    }
    void onPassKeyNotify(uint32_t pin) override {
        Serial.printf("[SEC] display PIN: %06lu\n", (unsigned long)pin);
    }
    void onAuthenticationComplete(ble_gap_conn_desc* desc) override {
        Serial.printf("[SEC] auth complete: bonded=%d encrypted=%d\n",
                      desc->sec_state.bonded, desc->sec_state.encrypted);
    }
};
static SecCB s_sec_cb;

struct ScanCB : public NimBLEAdvertisedDeviceCallbacks {
    void onResult(NimBLEAdvertisedDevice* dev) override {
        if (s_connected) return;
        bool hasHid = dev->isAdvertisingService(NimBLEUUID((uint16_t)0x1812));
        Serial.printf("[SCAN] %s  RSSI=%d  name='%s'  HID=%d\n",
                      dev->getAddress().toString().c_str(),
                      dev->getRSSI(),
                      dev->haveName() ? dev->getName().c_str() : "",
                      hasHid);

        // Only try connectable devices with decent signal
        if (!dev->isConnectable()) return;
        if (dev->getRSSI() < -75) return;  // too far, ignore

        std::string addrStr = dev->getAddress().toString();
        if (addr_on_cooldown(addrStr.c_str())) return;  // recently failed

        std::string name = dev->haveName() ? dev->getName() : "";
        if (name.find("[TV]") != std::string::npos) return;
        if (name.find("iPhone") != std::string::npos) return;
        if (name.find("Android") != std::string::npos) return;
        // Skip public-address devices that don't advertise HID
        // (smartphones/TVs use public addr; keyboards typically use random addr or advertise HID)
        if (dev->getAddress().getType() == 0 && !hasHid) return;

        AddrItem item;
        snprintf(item.buf, sizeof(item.buf), "%s|%d",
                 dev->getAddress().toString().c_str(),
                 (int)dev->getAddress().getType());
        xQueueSend(s_addr_queue, &item, 0);  // non-blocking, task context
        NimBLEDevice::getScan()->stop();
    }
};
static ScanCB s_scan_cb;

// ── Connection helpers ───────────────────────────────────────────────────────

static Preferences s_prefs;

static void save_address(const NimBLEAddress& addr) {
    s_prefs.begin(NVS_NAMESPACE, false);
    s_prefs.putString(NVS_KEY_ADDR, addr.toString().c_str());
    s_prefs.end();
    Serial.printf("[BLE] saved address %s\n", addr.toString().c_str());
}

static String load_address() {
    s_prefs.begin(NVS_NAMESPACE, true);
    String a = s_prefs.getString(NVS_KEY_ADDR, "");
    s_prefs.end();
    return a;
}

static bool subscribe_hid(NimBLEClient* c) {
    NimBLERemoteService* svc = c->getService(NimBLEUUID((uint16_t)0x1812));
    if (!svc) { Serial.println("[BLE] no HID service"); return false; }

    // Subscribe to all Report characteristics (UUID 0x2A4D)
    int sub_count = 0;
    auto chars = svc->getCharacteristics(true);
    for (auto* ch : *chars) {
        if (!ch->getUUID().equals(NimBLEUUID((uint16_t)0x2A4D))) continue;
        if (ch->canNotify()) {
            ch->subscribe(true, notify_cb);
            sub_count++;
            Serial.printf("[BLE] subscribed to report handle 0x%04X\n", ch->getHandle());
        }
    }
    Serial.printf("[BLE] subscribed to %d report(s)\n", sub_count);
    return sub_count > 0;
}

static bool try_connect(const NimBLEAddress& addr) {
    Serial.printf("[BLE] connecting to %s...\n", addr.toString().c_str());
    // Always create fresh client to avoid stale state from previous failed attempt
    if (s_client) {
        NimBLEDevice::deleteClient(s_client);
        s_client = nullptr;
    }
    s_client = NimBLEDevice::createClient();
    s_client->setClientCallbacks(&s_client_cb, false);
    s_client->setConnectionParams(12, 12, 0, 200);
    s_client->setConnectTimeout(5);  // 5s max per device

    if (!s_client->connect(addr)) {
        Serial.println("[BLE] connect failed");
        NimBLEDevice::deleteClient(s_client);
        s_client = nullptr;
        return false;
    }
    if (!subscribe_hid(s_client)) {
        s_client->disconnect();
        NimBLEDevice::deleteClient(s_client);
        s_client = nullptr;
        return false;
    }
    save_address(addr);
    return true;
}

// ── BLE management task ───────────────────────────────────────────────────────

static void ble_task(void* arg) {
    // 1. Try to reconnect to previously bonded device
    String stored = load_address();
    if (!stored.isEmpty()) {
        Serial.printf("[BLE] trying stored device %s\n", stored.c_str());
        NimBLEAddress addr(stored.c_str());
        for (int i = 0; i < 3 && !s_connected; i++) {
            if (try_connect(addr)) break;
            vTaskDelay(pdMS_TO_TICKS(2000));
        }
    }

    NimBLEScan* scan = NimBLEDevice::getScan();
    scan->setAdvertisedDeviceCallbacks(&s_scan_cb, false);
    scan->setActiveScan(true);
    scan->setInterval(100);  // 100 * 0.625ms = 62.5ms
    scan->setWindow(99);     // almost continuous

    while (true) {
        if (s_connected) { vTaskDelay(pdMS_TO_TICKS(500)); continue; }

        // 2. Start continuous scan; callback stops it and queues the address
        Serial.println("[BLE] scanning for keyboard...");
        xQueueReset(s_addr_queue);
        scan->start(0, false);  // 0 = scan until stop()

        // Wait for scan callback to deliver an address
        AddrItem item = {};
        while (!s_connected) {
            BaseType_t got = xQueueReceive(s_addr_queue, &item, pdMS_TO_TICKS(3000));
            if (got == pdTRUE) {
                // Parse "aa:bb:cc:dd:ee:ff|type"
                char* sep = strchr(item.buf, '|');
                uint8_t addr_type = 0;
                if (sep) { *sep = 0; addr_type = (uint8_t)atoi(sep + 1); }
                Serial.printf("[BLE] found candidate %s type=%d\n", item.buf, addr_type);
                NimBLEAddress addr(item.buf, addr_type);

                // Wait for scan to fully stop before switching to initiator mode
                vTaskDelay(pdMS_TO_TICKS(200));

                bool ok = false;
                for (int attempt = 1; attempt <= 3 && !ok; attempt++) {
                    Serial.printf("[BLE] connect attempt %d/3...\n", attempt);
                    ok = try_connect(addr);
                    if (!ok) vTaskDelay(pdMS_TO_TICKS(500));
                }
                if (ok) break;

                mark_fail(item.buf);
                Serial.println("[BLE] all attempts failed, resuming scan...");
                xQueueReset(s_addr_queue);
                if (!s_connected) scan->start(0, false);
            }
            // Timeout: scan still running, just keep waiting
        }
        if (!s_connected) scan->stop();
    }
}

// ── Public API ────────────────────────────────────────────────────────────────

void ble_hid_init() {
    s_addr_queue = xQueueCreate(4, sizeof(AddrItem));

    NimBLEDevice::init(BLE_DEVICE_NAME);
    // "Just Works" bonding: no PIN/passkey — we have no display or keyboard
    NimBLEDevice::setSecurityAuth(BLE_SM_PAIR_AUTHREQ_BOND | BLE_SM_PAIR_AUTHREQ_SC);
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);
    NimBLEDevice::setSecurityInitKey(BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID);
    NimBLEDevice::setSecurityRespKey(BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID);
    NimBLEDevice::setSecurityCallbacks(&s_sec_cb);
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);

    xTaskCreatePinnedToCore(ble_task, "ble_hid", 6144, nullptr, 5, nullptr, 0);
}

bool ble_hid_is_connected() { return s_connected; }
