#ifndef ZGOLF_SHOT_H
#define ZGOLF_SHOT_H

#include <stdint.h>

typedef struct { uint8_t charging; uint8_t wait_release; } shot_input_state_t;
enum { SHOT_INPUT_BEGIN=1, SHOT_INPUT_TICK=2, SHOT_INPUT_RELEASE=4, SHOT_INPUT_CANCEL=8 };
uint8_t shot_input_update(shot_input_state_t* state, uint16_t held, uint16_t pressed, uint16_t released, uint8_t ready);

void shot_begin_charge(void);
void shot_charge_tick(void);
uint8_t shot_power_segments(void);
uint8_t shot_power_level(void);
uint8_t shot_last_power_level(void);
int16_t shot_velocity_from_vector(int16_t vector);
void shot_count_stroke(void);
uint16_t shot_get_strokes(void);
void shot_reset_strokes(void);

#endif
