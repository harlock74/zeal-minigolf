#include <stdint.h>

#include <zgdk/input.h>

#include "config.h"
#include "shot.h"

static uint8_t shot_power_step = 0;
static uint8_t shot_charge_wait = 0;
static uint16_t strokes = 0;
static uint8_t last_power = 0;
/* Q2 gains. Levels 1-5 preserve fine positioning with constant rolling
 * resistance; levels 6-32 retain their original launch speeds. */
static const uint8_t shot_power_steps[SHOT_POWER_LEVELS] = {
    8,9,10,11, 12,14,17,20, 25,30,35,40, 46,52,58,64,
    71,78,85,92, 98,104,110,116, 122,128,134,140, 148,156,164,172
};

uint8_t shot_input_update(shot_input_state_t* state, uint16_t held, uint16_t pressed, uint16_t released, uint8_t ready)
{
    uint8_t events=0;
    if(!(held & SHOOT_BUTTON_MASK)) state->wait_release=0;
    if(!ready) {state->charging=0;return 0;}
    if(pressed & BUTTON_A) {
        events=state->charging ? SHOT_INPUT_CANCEL : 0;
        state->charging=0;
        state->wait_release=(uint8_t)((held & SHOOT_BUTTON_MASK)!=0);
        return events;
    }
    if(state->charging && (released & SHOOT_BUTTON_MASK)) {
        state->charging=0;
        state->wait_release=(uint8_t)((held & SHOOT_BUTTON_MASK)!=0);
        return SHOT_INPUT_RELEASE;
    }
    if(!state->charging && !state->wait_release && (pressed & SHOOT_BUTTON_MASK)) {
        state->charging=1;events=SHOT_INPUT_BEGIN;
    }
    if(state->charging) {
        if((held & SHOOT_BUTTON_MASK) && !(released & SHOOT_BUTTON_MASK)) events|=SHOT_INPUT_TICK;
        else {
            state->charging=0;
            state->wait_release=(uint8_t)((held & SHOOT_BUTTON_MASK)!=0);
            events|=SHOT_INPUT_RELEASE;
        }
    }
    return events;
}

void shot_begin_charge(void)
{
    shot_power_step = 0;
    shot_charge_wait = SHOT_CHARGE_TICKS;
}

void shot_charge_tick(void)
{
    if (shot_charge_wait > 0) {
        shot_charge_wait--;
        if (shot_charge_wait > 0) {
            return;
        }
    }

    shot_charge_wait = SHOT_CHARGE_TICKS;

    if (shot_power_step < (SHOT_POWER_LEVELS - 1)) {
        shot_power_step++;
    } /* Hold at full power until release. */
}

uint8_t shot_power_segments(void)
{
    return (uint8_t)((shot_power_step >> 2) + 1);
}

uint8_t shot_power_level(void) { return (uint8_t)(shot_power_step + 1); }
uint8_t shot_last_power_level(void) { return last_power; }

int16_t shot_velocity_from_vector(int16_t vector)
{
    int16_t magnitude = vector < 0 ? (int16_t)(0 - vector) : vector;
    int16_t speed = (int16_t)((magnitude * shot_power_steps[shot_power_step]) >> 3);
    return vector < 0 ? (int16_t)(0 - speed) : speed;
}

void shot_count_stroke(void)
{
    strokes++;
    last_power = shot_power_level();
}

uint16_t shot_get_strokes(void)
{
    return strokes;
}

void shot_reset_strokes(void)
{
    strokes = 0;
    last_power = 0;
}
