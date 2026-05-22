#include <stdint.h>

#include <zgdk/input.h>

#include "config.h"
#include "ramp.h"

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

void ramp_update(void)
{
    if (ramp_cooldown > 0) {
        ramp_cooldown--;
    }

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
    uint8_t half;
    uint8_t progress;

    if (!ramp_is_airborne() || ramp_air_total < 2) {
        return 0;
    }

    half = (uint8_t)(ramp_air_total >> 1);
    progress = (uint8_t)(ramp_air_total - ramp_air_frames);
    if (progress > half) {
        progress = (uint8_t)(ramp_air_total - progress);
    }

    return (uint8_t)((progress * RAMP_ARC_MAX_PIXELS) / half);
}

void ramp_start_jump(uint8_t speed, int16_t* vx, int16_t* vy)
{
    uint8_t air_frames = (uint8_t)(speed >> RAMP_AIR_SPEED_SHIFT);

    if (air_frames < RAMP_MIN_AIR_FRAMES) {
        air_frames = RAMP_MIN_AIR_FRAMES;
    }
    if (air_frames > RAMP_MAX_AIR_FRAMES) {
        air_frames = RAMP_MAX_AIR_FRAMES;
    }

    ramp_air_frames = air_frames;
    ramp_air_total = air_frames;
    ramp_cooldown = RAMP_COOLDOWN_FRAMES;
    *vx = (int16_t)((*vx * RAMP_LAUNCH_SPEED_NUMERATOR) >> RAMP_LAUNCH_SPEED_SHIFT);
    *vy = (int16_t)((*vy * RAMP_LAUNCH_SPEED_NUMERATOR) >> RAMP_LAUNCH_SPEED_SHIFT);
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
    if (speed < RAMP_FAILED_MIN_REBOUND_SPEED) {
        speed = RAMP_FAILED_MIN_REBOUND_SPEED;
    }

    if (velocity < 0) {
        return (int16_t)speed;
    }

    return (int16_t)(0 - speed);
}
