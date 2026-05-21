#ifndef ZGOLF_SHOT_H
#define ZGOLF_SHOT_H

#include <stdint.h>

void shot_begin_charge(void);
void shot_charge_tick(void);
uint8_t shot_power_segments(void);
int16_t shot_velocity_from_vector(int16_t vector);
void shot_count_stroke(void);
uint16_t shot_get_strokes(void);
void shot_reset_strokes(void);

#endif
