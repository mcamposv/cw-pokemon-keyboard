#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// HID Usage ID → PS/2 Set 2 make code
//   0x0000  : not mapped
//   0x00XX  : single-byte make code XX
//   0x01XX  : extended key (send 0xE0, then XX)
extern const uint16_t HID_TO_PS2[256];

// HID modifier bitmask bit N → PS/2 make code
extern const uint16_t MODIFIER_TO_PS2[8];

#ifdef __cplusplus
}
#endif
