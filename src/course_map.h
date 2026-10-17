#ifndef ZGOLF_COURSE_MAP_H
#define ZGOLF_COURSE_MAP_H
#include <stdint.h>
#include "config.h"
/* SDK RLE: validate the whole 40x30 map, retain its 40x24 playfield. */
static inline uint8_t course_map_decode(const uint8_t* source,uint16_t size,uint8_t* dest)
{
    uint16_t position=0;
    while(position<COURSE_TILES_W*COURSE_TILES_H) {
        uint8_t control,count,tile=0;
        if(!size)return 0;
        control=*source++;size--;count=(control&127)+1;
        if(position+count>COURSE_TILES_W*COURSE_TILES_H)return 0;
        if(control&128){if(!size)return 0;tile=*source++;size--;}
        else if(size<count)return 0;
        while(count--) {
            uint8_t value=tile;
            if(!(control&128)){value=*source++;size--;}
            if(position<COURSE_TILES_W*PLAYFIELD_TILES_H)dest[position]=value;
            position++;
        }
    }
    return size==0;
}
#endif
