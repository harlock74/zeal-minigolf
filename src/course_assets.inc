/* Link SDK course maps. Sizes are resolved by the assembler from start/end labels. */
#include "course_assets.h"

uint8_t* course_maps[] = {
    &_course_map0_start,
    &_course_map1_start,
    &_course_map2_start,
    &_course_map3_start,
    &_course_map4_start,
    &_course_map5_start,
    &_course_map6_start,
    &_course_map7_start,
    &_course_map8_start,
    &_course_map9_start,
    &_course_map10_start,
    &_course_map11_start,
    &_course_map12_start,
    &_course_map13_start,
    &_course_map14_start,
};
uint8_t course_map_count = sizeof(course_maps) / sizeof(course_maps[0]);

