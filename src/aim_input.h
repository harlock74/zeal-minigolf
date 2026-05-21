#ifndef ZGOLF_AIM_INPUT_H
#define ZGOLF_AIM_INPUT_H

#include <stdint.h>

typedef struct {
    uint8_t repeat_wait;
    uint8_t held_steps;
    uint16_t held_mask;
    uint16_t blocked_mask;
} aim_input_state_t;

void aim_input_reset(aim_input_state_t* state);
uint16_t aim_input_clear(aim_input_state_t* state, uint16_t input, uint16_t* previous);
uint16_t aim_input_filter(aim_input_state_t* state, uint16_t input, uint8_t ball_rolling);
void aim_input_tick(aim_input_state_t* state, uint16_t input);
uint8_t aim_input_should_rotate_left(
    const aim_input_state_t* state,
    uint16_t input,
    uint16_t previous,
    uint8_t ball_rolling);
uint8_t aim_input_should_rotate_right(
    const aim_input_state_t* state,
    uint16_t input,
    uint16_t previous,
    uint8_t ball_rolling);
void aim_input_note_rotation(aim_input_state_t* state, uint16_t direction);

#endif
