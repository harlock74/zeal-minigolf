#include <stdint.h>

#include <zgdk/input.h>

#include "config.h"
#include "shot.h"

static uint8_t shot_power_step = 0;
static uint8_t shot_charge_wait = 0;
static uint16_t strokes = 0;
static const uint8_t shot_power_steps[POWER_BAR_SEGMENTS] = SHOT_POWER_STEPS;

void shot_begin_charge(void)
{
    shot_power_step = 0;
    shot_charge_wait = POWER_BAR_STEP_FRAMES;
}

void shot_charge_tick(void)
{
    if (shot_charge_wait > 0) {
        shot_charge_wait--;
        return;
    }

    shot_charge_wait = POWER_BAR_STEP_FRAMES;

    if (shot_power_step < (POWER_BAR_SEGMENTS - 1)) {
        shot_power_step++;
    } else {
        shot_power_step = 0;
    }
}

uint8_t shot_power_segments(void)
{
    return (uint8_t)(shot_power_step + 1);
}

int16_t shot_velocity_from_vector(int16_t vector)
{
    return (int16_t)((vector * shot_power_steps[shot_power_step]) >> SHOT_VELOCITY_SHIFT);
}

void shot_count_stroke(void)
{
    strokes++;
}

uint16_t shot_get_strokes(void)
{
    return strokes;
}

void shot_reset_strokes(void)
{
    strokes = 0;
}
