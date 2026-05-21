#ifndef ZGOLF_TERRAIN_H
#define ZGOLF_TERRAIN_H

#include <stdint.h>

uint8_t terrain_tile_is_water(uint8_t tile);
uint8_t terrain_tile_is_slash_deflector(uint8_t tile);
uint8_t terrain_tile_is_backslash_deflector(uint8_t tile);
uint8_t terrain_tile_is_deflector(uint8_t tile);

#endif
