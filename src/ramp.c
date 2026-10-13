#include <stdint.h>

#include <zgdk/input.h>

#include "config.h"
#include "ramp.h"

#if RAMP_AIR_FRAMES != 24 || RAMP_ARC_MAX_PIXELS != 12
#error "Update the ramp arc lookup table when changing duration or height"
#endif

static uint8_t ramp_air_frames = 0;
static uint8_t ramp_air_total = 0;
static uint8_t ramp_cooldown = 0;

static int16_t ramp_abs_i16(int16_t value)
{
    if (value < 0) {
        return (int16_t)-value;
    }

    return value;
}

void ramp_reset(void)
{
    ramp_air_frames = 0;
    ramp_air_total = 0;
    ramp_cooldown = 0;
}

void ramp_tick_cooldown(void)
{
    if (ramp_cooldown > 0) {
        ramp_cooldown--;
    }
}

void ramp_update(void)
{
    if (ramp_air_frames > 0) {
        ramp_air_frames--;
        if (ramp_air_frames == 0) {
            ramp_air_total = 0;
        }
    }
}

uint8_t ramp_is_airborne(void)
{
    return (uint8_t)(ramp_air_frames > 0);
}

uint8_t ramp_can_start(void)
{
    return (uint8_t)(!ramp_is_airborne() && ramp_cooldown == 0);
}

uint8_t ramp_tile_is(uint8_t tile)
{
    return (uint8_t)(
        tile == TILE_ID_RAMP_UP_LEFT ||
        tile == TILE_ID_RAMP_UP_RIGHT ||
        tile == TILE_ID_RAMP_RIGHT_LEFT ||
        tile == TILE_ID_RAMP_RIGHT_RIGHT);
}

uint8_t ramp_accepts_direction(uint8_t tile, int16_t vx, int16_t vy)
{
    if (tile == TILE_ID_RAMP_UP_LEFT || tile == TILE_ID_RAMP_UP_RIGHT) {
        return (uint8_t)(vy < 0 && ramp_abs_i16(vy) >= ramp_abs_i16(vx));
    }

    if (tile == TILE_ID_RAMP_RIGHT_LEFT || tile == TILE_ID_RAMP_RIGHT_RIGHT) {
        return (uint8_t)(vx > 0 && ramp_abs_i16(vx) >= ramp_abs_i16(vy));
    }

    return 0;
}

uint8_t ramp_arc_offset(void)
{
    /* A 24-frame parabola: no division or large products during rendering. */
    static const uint8_t arc[] = { 0, 2, 4, 5, 7, 8, 9, 10, 11, 11, 12, 12,
        12, 12, 12, 11, 11, 10, 9, 8, 7, 5, 4, 2, 0 };
    if (!ramp_is_airborne()) {
        return 0;
    }
    return arc[ramp_air_total - ramp_air_frames];
}

void ramp_start_jump(uint8_t speed, int16_t* vx, int16_t* vy)
{
    /* Entry speed controls distance; duration is consistent for every ramp. */
    (void)speed;
    ramp_air_frames = RAMP_AIR_FRAMES;
    ramp_air_total = RAMP_AIR_FRAMES;
    ramp_cooldown = RAMP_COOLDOWN_FRAMES;
    *vx = (int16_t)(*vx - *vx / (1 << RAMP_LAUNCH_LOSS_SHIFT));
    *vy = (int16_t)(*vy - *vy / (1 << RAMP_LAUNCH_LOSS_SHIFT));
}

void ramp_start_failed_rebound(void)
{
    ramp_cooldown = RAMP_FAILED_COOLDOWN_FRAMES;
}

int16_t ramp_failed_rebound_velocity(int16_t velocity)
{
    uint16_t speed = (uint16_t)ramp_abs_i16(velocity);

    if (speed == 0) {
        return 0;
    }

    speed = (uint16_t)((speed * RAMP_FAILED_REBOUND_NUMERATOR) >> RAMP_FAILED_REBOUND_SHIFT);
    if (speed <= BALL_STOP_SPEED) {
        return 0;
    }

    if (velocity < 0) {
        return (int16_t)speed;
    }

    return (int16_t)(0 - speed);
}
