#include <stdint.h>

#include <zgdk/input.h>

#include "config.h"
#include "aim_input.h"

void aim_input_reset(aim_input_state_t* state)
{
    state->repeat_wait = 0;
}

uint16_t aim_input_clear(aim_input_state_t* state, uint16_t input, uint16_t* previous)
{
    const uint16_t aim_mask = BUTTON_LEFT | BUTTON_RIGHT;

    *previous &= (uint16_t)~aim_mask;
    aim_input_reset(state);

    return (uint16_t)(input & (uint16_t)~aim_mask);
}

uint16_t aim_input_filter(aim_input_state_t* state, uint16_t input, uint8_t ball_rolling)
{
    const uint16_t aim_mask = BUTTON_LEFT | BUTTON_RIGHT;
    (void)state;
    (void)ball_rolling;
    if (input & SHOOT_BUTTON_MASK) {
        return (uint16_t)(input & (uint16_t)~aim_mask);
    }
    return input;
}

void aim_input_tick(aim_input_state_t* state, uint16_t input)
{
    if (state->repeat_wait > 0) {
        state->repeat_wait--;
    }

    if (!(input & (BUTTON_LEFT | BUTTON_RIGHT)) ||
        ((input & BUTTON_LEFT) && (input & BUTTON_RIGHT))) {
        state->repeat_wait = 0;
    }
}

uint8_t aim_input_should_rotate_left(
    const aim_input_state_t* state,
    uint16_t input,
    uint16_t previous,
    uint8_t ball_rolling)
{
    if (ball_rolling || ((input & BUTTON_LEFT) && (input & BUTTON_RIGHT))) {
        return 0;
    }

    if ((input & BUTTON_LEFT) && !(previous & BUTTON_LEFT)) {
        return 1;
    }

    return (uint8_t)((input & BUTTON_LEFT) &&
        !(input & BUTTON_RIGHT) &&
        state->repeat_wait == 0);
}

uint8_t aim_input_should_rotate_right(
    const aim_input_state_t* state,
    uint16_t input,
    uint16_t previous,
    uint8_t ball_rolling)
{
    if (ball_rolling || ((input & BUTTON_LEFT) && (input & BUTTON_RIGHT))) {
        return 0;
    }

    if ((input & BUTTON_RIGHT) && !(previous & BUTTON_RIGHT)) {
        return 1;
    }

    return (uint8_t)((input & BUTTON_RIGHT) &&
        !(input & BUTTON_LEFT) &&
        state->repeat_wait == 0);
}

void aim_input_note_rotation(aim_input_state_t* state, uint16_t direction)
{
    if (direction == 0) {
        return;
    }

    /* Lost keyboard releases are handled by the event lease in input.c.
     * A real controller may legitimately stay held for any duration. */
    state->repeat_wait = AIM_INPUT_REPEAT_FRAMES;
}
