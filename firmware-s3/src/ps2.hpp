#pragma once
#include <stdint.h>

void ps2_init();
void ps2_key_down(uint16_t ps2_code);  // 0x00XX = normal, 0x01XX = extended
void ps2_key_up(uint16_t ps2_code);
