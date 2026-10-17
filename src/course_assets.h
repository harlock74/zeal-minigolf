/* SDK map symbols; no map sizes or tile bytes are maintained by hand. */
#pragma once
#include <stdint.h>

extern uint8_t _course_map0_start;
extern uint8_t _course_map0_end;
extern uint8_t _course_map1_start;
extern uint8_t _course_map1_end;
extern uint8_t _course_map2_start;
extern uint8_t _course_map2_end;
extern uint8_t _course_map3_start;
extern uint8_t _course_map3_end;
extern uint8_t _course_map4_start;
extern uint8_t _course_map4_end;
extern uint8_t _course_map5_start;
extern uint8_t _course_map5_end;
extern uint8_t _course_map6_start;
extern uint8_t _course_map6_end;
extern uint8_t _course_map7_start;
extern uint8_t _course_map7_end;
extern uint8_t _course_map8_start;
extern uint8_t _course_map8_end;
extern uint8_t _course_map9_start;
extern uint8_t _course_map9_end;
extern uint8_t _course_map10_start;
extern uint8_t _course_map10_end;
extern uint8_t _course_map11_start;
extern uint8_t _course_map11_end;
extern uint8_t _course_map12_start;
extern uint8_t _course_map12_end;
extern uint8_t _course_map13_start;
extern uint8_t _course_map13_end;
extern uint8_t _course_map14_start;
extern uint8_t _course_map14_end;

extern uint8_t* course_maps[];
extern uint8_t course_map_count;
extern const uint16_t course_map_sizes[];

#define COURSE_MAPS_ASM \
    "__course_map0_start:\n" \
    "    .incbin \"assets/minigolf0000.ztm\"\n" \
    "__course_map0_end:\n" \
    "__course_map1_start:\n" \
    "    .incbin \"assets/minigolf0001.ztm\"\n" \
    "__course_map1_end:\n" \
    "__course_map2_start:\n" \
    "    .incbin \"assets/minigolf0002.ztm\"\n" \
    "__course_map2_end:\n" \
    "__course_map3_start:\n" \
    "    .incbin \"assets/minigolf0003.ztm\"\n" \
    "__course_map3_end:\n" \
    "__course_map4_start:\n" \
    "    .incbin \"assets/minigolf0004.ztm\"\n" \
    "__course_map4_end:\n" \
    "__course_map5_start:\n" \
    "    .incbin \"assets/minigolf0005.ztm\"\n" \
    "__course_map5_end:\n" \
    "__course_map6_start:\n" \
    "    .incbin \"assets/minigolf0006.ztm\"\n" \
    "__course_map6_end:\n" \
    "__course_map7_start:\n" \
    "    .incbin \"assets/minigolf0007.ztm\"\n" \
    "__course_map7_end:\n" \
    "__course_map8_start:\n" \
    "    .incbin \"assets/minigolf0008.ztm\"\n" \
    "__course_map8_end:\n" \
    "__course_map9_start:\n" \
    "    .incbin \"assets/minigolf0009.ztm\"\n" \
    "__course_map9_end:\n" \
    "__course_map10_start:\n" \
    "    .incbin \"assets/minigolf0010.ztm\"\n" \
    "__course_map10_end:\n" \
    "__course_map11_start:\n" \
    "    .incbin \"assets/minigolf0011.ztm\"\n" \
    "__course_map11_end:\n" \
    "__course_map12_start:\n" \
    "    .incbin \"assets/minigolf0012.ztm\"\n" \
    "__course_map12_end:\n" \
    "__course_map13_start:\n" \
    "    .incbin \"assets/minigolf0013.ztm\"\n" \
    "__course_map13_end:\n" \
    "__course_map14_start:\n" \
    "    .incbin \"assets/minigolf0014.ztm\"\n" \
    "__course_map14_end:\n" \
    ".globl _course_map_sizes\n_course_map_sizes:\n" \
    "    .dw __course_map0_end-__course_map0_start\n" \
    "    .dw __course_map1_end-__course_map1_start\n" \
    "    .dw __course_map2_end-__course_map2_start\n" \
    "    .dw __course_map3_end-__course_map3_start\n" \
    "    .dw __course_map4_end-__course_map4_start\n" \
    "    .dw __course_map5_end-__course_map5_start\n" \
    "    .dw __course_map6_end-__course_map6_start\n" \
    "    .dw __course_map7_end-__course_map7_start\n" \
    "    .dw __course_map8_end-__course_map8_start\n" \
    "    .dw __course_map9_end-__course_map9_start\n" \
    "    .dw __course_map10_end-__course_map10_start\n" \
    "    .dw __course_map11_end-__course_map11_start\n" \
    "    .dw __course_map12_end-__course_map12_start\n" \
    "    .dw __course_map13_end-__course_map13_start\n" \
    "    .dw __course_map14_end-__course_map14_start\n"
