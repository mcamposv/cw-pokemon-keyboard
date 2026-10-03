#include "usb_kbd.hpp"
#include "keymap.h"
#include "ps2.hpp"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

extern "C" {
#include "usb/usb_host.h"
}

// ── State ─────────────────────────────────────────────────────────────────────

static volatile bool             s_active   = false;
static usb_host_client_handle_t  s_client   = nullptr;
static usb_device_handle_t       s_dev      = nullptr;
static usb_transfer_t*           s_xfer     = nullptr;
static uint8_t                   s_intf_num = 0;
static TaskHandle_t              s_lib_task = nullptr;

struct HidRep { uint8_t mod; uint8_t keys[6]; };
static HidRep s_prev = {};

bool usb_kbd_is_active() { return s_active; }

// ── HID keycode → nombre legible ─────────────────────────────────────────────

static const char* hid_name(uint8_t code) {
    if (code >= 0x04 && code <= 0x1D) {
        static const char* letters[] = {
            "a","b","c","d","e","f","g","h","i","j","k","l","m",
            "n","o","p","q","r","s","t","u","v","w","x","y","z"
        };
        return letters[code - 0x04];
    }
    switch (code) {
        case 0x1E: return "1";      case 0x1F: return "2";      case 0x20: return "3";
        case 0x21: return "4";      case 0x22: return "5";      case 0x23: return "6";
        case 0x24: return "7";      case 0x25: return "8";      case 0x26: return "9";
        case 0x27: return "0";
        case 0x28: return "ENTER";  case 0x29: return "ESC";    case 0x2A: return "BKSP";
        case 0x2B: return "TAB";    case 0x2C: return "SPACE";
        case 0x2D: return "-";      case 0x2E: return "=";
        case 0x36: return ",";      case 0x37: return ".";      case 0x38: return "/";
        case 0x39: return "CAPS";
        case 0x3A: return "F1";     case 0x3B: return "F2";     case 0x3C: return "F3";
        case 0x3D: return "F4";     case 0x3E: return "F5";     case 0x3F: return "F6";
        case 0x40: return "F7";     case 0x41: return "F8";     case 0x42: return "F9";
        case 0x43: return "F10";    case 0x44: return "F11";    case 0x45: return "F12";
        case 0x4A: return "HOME";   case 0x4D: return "END";
        case 0x4B: return "PGUP";   case 0x4E: return "PGDN";
        case 0x4C: return "DEL";    case 0x49: return "INS";
        case 0x4F: return "RIGHT";  case 0x50: return "LEFT";
        case 0x51: return "DOWN";   case 0x52: return "UP";
        default:   return "?";
    }
}

static const char* mod_name(int bit) {
    switch (bit) {
        case 0: return "L-CTRL";  case 1: return "L-SHIFT";
        case 2: return "L-ALT";   case 3: return "L-GUI";
        case 4: return "R-CTRL";  case 5: return "R-SHIFT";
        case 6: return "R-ALT";   case 7: return "R-GUI";
        default: return "?";
    }
}

// ── Key processing ────────────────────────────────────────────────────────────

static void process_rep(const uint8_t* buf, int len) {
    if (len < 3) return;
    HidRep r = {};
    r.mod = buf[0];
    for (int i = 0; i < 6 && (i + 2) < len; i++) r.keys[i] = buf[i + 2];

    uint8_t chg = r.mod ^ s_prev.mod;
    for (int i = 0; i < 8; i++) {
        if (!(chg & (1 << i))) continue;
        uint16_t c = MODIFIER_TO_PS2[i];
        if (!c) continue;
        if (r.mod & (1 << i)) {
            Serial.printf("[KBD] + %-8s (HID=0xE%d PS2=0x%04X)\n", mod_name(i), i, c);
            ps2_key_down(c);
        } else {
            Serial.printf("[KBD] - %-8s (HID=0xE%d PS2=0x%04X)\n", mod_name(i), i, c);
            ps2_key_up(c);
        }
    }
    for (int i = 0; i < 6; i++) {
        if (!s_prev.keys[i]) continue;
        bool held = false;
        for (int j = 0; j < 6; j++) if (r.keys[j] == s_prev.keys[i]) { held = true; break; }
        if (!held && HID_TO_PS2[s_prev.keys[i]]) {
            Serial.printf("[KBD] ^ %-6s (HID=0x%02X PS2=0x%04X)\n", hid_name(s_prev.keys[i]), s_prev.keys[i], HID_TO_PS2[s_prev.keys[i]]);
            ps2_key_up(HID_TO_PS2[s_prev.keys[i]]);
        }
    }
    for (int i = 0; i < 6; i++) {
        if (!r.keys[i]) continue;
        bool was = false;
        for (int j = 0; j < 6; j++) if (s_prev.keys[j] == r.keys[i]) { was = true; break; }
        if (!was && HID_TO_PS2[r.keys[i]]) {
            Serial.printf("[KBD] v %-6s (HID=0x%02X PS2=0x%04X)\n", hid_name(r.keys[i]), r.keys[i], HID_TO_PS2[r.keys[i]]);
            ps2_key_down(HID_TO_PS2[r.keys[i]]);
        }
    }
    s_prev = r;
}

// ── Transfer callback ─────────────────────────────────────────────────────────

static void xfer_cb(usb_transfer_t* xfer) {
    if (xfer->status == USB_TRANSFER_STATUS_COMPLETED && xfer->actual_num_bytes > 0) {
        const uint8_t* d = xfer->data_buffer;
        int n = xfer->actual_num_bytes;
        // Boot protocol = 8 bytes [mod][0][k0..k5]
        // Some devices prefix with report-ID byte
        if (n >= 9 && d[0] != 0) process_rep(d + 1, n - 1);
        else                      process_rep(d, n);
    }
    if (s_active && s_dev) usb_host_transfer_submit(xfer);
}

// ── Interface setup ───────────────────────────────────────────────────────────

static bool claim_hid_keyboard(usb_device_handle_t dev) {
    const usb_config_desc_t* cfg;
    if (usb_host_get_active_config_descriptor(dev, &cfg) != ESP_OK) return false;

    const uint8_t* p = (const uint8_t*)cfg;
    int total = cfg->wTotalLength;
    bool in_kbd_intf = false;
    uint8_t intf_num = 0;
    uint8_t ep_addr  = 0;
    uint16_t ep_mps  = 8;

    for (int i = 0; i < total; ) {
        if (i + 1 >= total) break;
        uint8_t dlen = p[i], dtype = p[i + 1];
        if (!dlen) break;

        if (dtype == 0x04) {  // INTERFACE descriptor
            const usb_intf_desc_t* id = (const usb_intf_desc_t*)(p + i);
            in_kbd_intf = (id->bInterfaceClass    == USB_CLASS_HID &&
                           id->bInterfaceSubClass  == 1 &&
                           id->bInterfaceProtocol  == 1);
            if (in_kbd_intf) intf_num = id->bInterfaceNumber;
        }
        if (in_kbd_intf && dtype == 0x05) {  // ENDPOINT descriptor
            const usb_ep_desc_t* ed = (const usb_ep_desc_t*)(p + i);
            if ((ed->bEndpointAddress & 0x80) &&
                (ed->bmAttributes & 0x03) == 0x03) {  // interrupt IN
                ep_addr = ed->bEndpointAddress;
                ep_mps  = ed->wMaxPacketSize;
                break;
            }
        }
        i += dlen;
    }

    if (!ep_addr) {
        // Fallback: try any HID interface (some keyboards don't set boot subclass)
        in_kbd_intf = false;
        for (int i = 0; i < total; ) {
            if (i + 1 >= total) break;
            uint8_t dlen = p[i], dtype = p[i + 1];
            if (!dlen) break;
            if (dtype == 0x04) {
                const usb_intf_desc_t* id = (const usb_intf_desc_t*)(p + i);
                in_kbd_intf = (id->bInterfaceClass == USB_CLASS_HID);
                if (in_kbd_intf) intf_num = id->bInterfaceNumber;
            }
            if (in_kbd_intf && dtype == 0x05) {
                const usb_ep_desc_t* ed = (const usb_ep_desc_t*)(p + i);
                if ((ed->bEndpointAddress & 0x80) && (ed->bmAttributes & 0x03) == 0x03) {
                    ep_addr = ed->bEndpointAddress;
                    ep_mps  = ed->wMaxPacketSize;
                    break;
                }
            }
            i += dlen;
        }
    }

    if (!ep_addr) { Serial.println("[USB] no HID interrupt-IN endpoint found"); return false; }

    Serial.printf("[USB] HID keyboard intf=%d ep=0x%02X mps=%d\n", intf_num, ep_addr, ep_mps);

    if (usb_host_interface_claim(s_client, dev, intf_num, 0) != ESP_OK) {
        Serial.println("[USB] interface claim failed"); return false;
    }
    s_intf_num = intf_num;

    if (usb_host_transfer_alloc(ep_mps, 0, &s_xfer) != ESP_OK) {
        Serial.println("[USB] transfer alloc failed"); return false;
    }
    s_xfer->device_handle    = dev;
    s_xfer->bEndpointAddress = ep_addr;
    s_xfer->callback         = xfer_cb;
    s_xfer->context          = nullptr;
    s_xfer->num_bytes        = ep_mps;

    if (usb_host_transfer_submit(s_xfer) != ESP_OK) {
        Serial.println("[USB] transfer submit failed");
        usb_host_transfer_free(s_xfer); s_xfer = nullptr; return false;
    }

    s_active = true;
    Serial.println("[USB] keyboard active!");
    return true;
}

static void release_device() {
    s_active = false;
    s_prev   = {};
    if (s_xfer) { usb_host_transfer_free(s_xfer); s_xfer = nullptr; }
    if (s_dev) {
        usb_host_interface_release(s_client, s_dev, s_intf_num);
        usb_host_device_close(s_client, s_dev);
        s_dev = nullptr;
    }
    Serial.println("[USB] keyboard disconnected");
}

// ── Client event callback (called from client task) ───────────────────────────

static void client_event_cb(const usb_host_client_event_msg_t* msg, void*) {
    switch (msg->event) {
    case USB_HOST_CLIENT_EVENT_NEW_DEV: {
        uint8_t addr = msg->new_dev.address;
        Serial.printf("[USB] device connected addr=%d\n", addr);
        usb_device_handle_t dev;
        if (usb_host_device_open(s_client, addr, &dev) == ESP_OK) {
            const usb_device_desc_t* desc;
            usb_host_get_device_descriptor(dev, &desc);
            Serial.printf("[USB] VID=%04X PID=%04X class=%d\n",
                          desc->idVendor, desc->idProduct, desc->bDeviceClass);
            s_dev = dev;
            if (!claim_hid_keyboard(dev)) {
                usb_host_device_close(s_client, dev);
                s_dev = nullptr;
            }
        }
        break;
    }
    case USB_HOST_CLIENT_EVENT_DEV_GONE:
        release_device();
        break;
    }
}

// ── Task 1: USB host library events ──────────────────────────────────────────

static void usb_lib_event_task(void*) {
    while (true) {
        uint32_t flags = 0;
        usb_host_lib_handle_events(portMAX_DELAY, &flags);
        if (flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) usb_host_device_free_all();
    }
}

// ── Task 2: USB client events ─────────────────────────────────────────────────

static void usb_client_task(void*) {
    usb_host_client_config_t ccfg = {};
    ccfg.is_synchronous              = false;
    ccfg.max_num_event_msg           = 5;
    ccfg.async.client_event_callback = client_event_cb;
    ccfg.async.callback_arg          = nullptr;

    if (usb_host_client_register(&ccfg, &s_client) != ESP_OK) {
        Serial.println("[USB] client register failed");
        vTaskDelete(nullptr); return;
    }
    Serial.println("[USB] host ready, waiting for keyboard...");

    while (true) {
        usb_host_client_handle_events(s_client, portMAX_DELAY);
    }
}

// ── Public API ────────────────────────────────────────────────────────────────

void usb_kbd_init() {
    usb_host_config_t cfg = {};
    cfg.skip_phy_setup = false;
    cfg.intr_flags     = ESP_INTR_FLAG_LEVEL1;

    if (usb_host_install(&cfg) != ESP_OK) {
        Serial.println("[USB] host install failed — check USB OTG port");
        return;
    }

    // Both tasks pinned to Core 0 alongside BLE
    xTaskCreatePinnedToCore(usb_lib_event_task, "usb_lib",    3072, nullptr, 2, &s_lib_task, 0);
    xTaskCreatePinnedToCore(usb_client_task,    "usb_client", 3072, nullptr, 2, nullptr,     0);
}
