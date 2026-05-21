#ifndef ZGOLF_AIM_H
#define ZGOLF_AIM_H

#include <stdint.h>

void aim_rotate_left(void);
void aim_rotate_right(void);
int16_t aim_vector_x(void);
int16_t aim_vector_y(void);
uint16_t aim_marker_coord(uint16_t origin, int16_t vector, uint16_t distance);

#endif
