#include <stdint.h>

#include <zgdk/input.h>

#include "config.h"
#include "coords.h"

uint16_t coords_sprite_from_tile(uint8_t tile)
{
    /*
     * ZVB sprite coordinates are biased by 16: coordinate 16 appears at
     * screen pixel 0. Keep the bias explicit so gameplay math can use pixels.
     */
    return (uint16_t)((tile * TILE_PIXEL_SIZE) + TILE_PIXEL_SIZE);
}

uint16_t coords_fixed_to_sprite(uint16_t value)
{
    return (uint16_t)(value >> FIXED_SHIFT);
}

uint16_t coords_fixed_to_center_pixel(uint16_t value)
{
    uint16_t coord = coords_fixed_to_sprite(value);

    if (coord > (TILE_PIXEL_SIZE / 2)) {
        coord = (uint16_t)(coord - (TILE_PIXEL_SIZE / 2));
    } else {
        coord = 0;
    }

    return coord;
}

uint8_t coords_fixed_to_center_tile(uint16_t value)
{
    /* floor(max(floor(value/16)-8,0)/16), with no nested conversions.
     * With Q4 positions and 16px tiles the final shift selects one byte. */
    if (value < (TILE_PIXEL_SIZE / 2) * FIXED_ONE) {
        return 0;
    }
    return (uint8_t)((value - (TILE_PIXEL_SIZE / 2) * FIXED_ONE) >>
        (FIXED_SHIFT + TILE_PIXEL_SHIFT));
}

uint16_t coords_center_pixel_to_fixed(int16_t value)
{
    if (value < 0) {
        value = 0;
    }

    return (uint16_t)(((uint16_t)value + (TILE_PIXEL_SIZE / 2)) << FIXED_SHIFT);
}
