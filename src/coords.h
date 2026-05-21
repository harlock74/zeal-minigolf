#ifndef ZGOLF_COORDS_H
#define ZGOLF_COORDS_H

#include <stdint.h>

uint16_t coords_sprite_from_tile(uint8_t tile);
uint16_t coords_fixed_to_sprite(uint16_t value);
uint16_t coords_fixed_to_center_pixel(uint16_t value);
uint8_t coords_fixed_to_center_tile(uint16_t value);
uint16_t coords_center_pixel_to_fixed(int16_t value);

#endif
