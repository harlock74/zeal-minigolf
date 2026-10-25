#include <stdint.h>

#include <zgdk/input.h>

#include "config.h"
#include "terrain.h"

uint8_t terrain_tile_is_water(uint8_t tile)
{
    return (uint8_t)(tile >= TILE_ID_WATER_FIRST && tile <= TILE_ID_WATER_LAST);
}

uint8_t terrain_tile_is_slash_deflector(uint8_t tile)
{
    return (uint8_t)(tile == TILE_ID_DEFLECT_SLASH_1 || tile == TILE_ID_DEFLECT_SLASH_2);
}

uint8_t terrain_tile_is_backslash_deflector(uint8_t tile)
{
    return (uint8_t)(tile == TILE_ID_DEFLECT_BACKSLASH_1 || tile == TILE_ID_DEFLECT_BACKSLASH_2);
}

uint8_t terrain_tile_is_deflector(uint8_t tile)
{
    /* These four atlas IDs are contiguous. Avoid two out-of-line predicate
     * calls for each of the ball's collision samples on the Z80. */
    return (uint8_t)(tile >= TILE_ID_DEFLECT_SLASH_1 &&
        tile <= TILE_ID_DEFLECT_BACKSLASH_2);
}
