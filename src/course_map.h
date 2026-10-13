#ifndef ZGOLF_COURSE_MAP_H
#define ZGOLF_COURSE_MAP_H
#include <stdint.h>
#include "config.h"

/* Runs are count/tile pairs. Decode once on course load, not during play. */
static inline uint8_t course_map_decode(const uint8_t* source, uint16_t size, uint8_t* dest)
{
    uint16_t position = 0;
    while (position < COURSE_TILES_W * PLAYFIELD_TILES_H) {
        uint8_t count, tile;
        if (size < 2) return 0;
        count = *source++; tile = *source++; size -= 2;
        if (!count || count > COURSE_TILES_W * PLAYFIELD_TILES_H - position) return 0;
        while (count--) dest[position++] = tile;
    }
    return size == 0;
}
#endif
