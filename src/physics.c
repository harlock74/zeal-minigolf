#include <stdint.h>

#include <zgdk/input.h>

#include "config.h"
#include "physics.h"

int16_t physics_abs_i16(int16_t value)
{
    if (value < 0) {
        return (int16_t)-value;
    }

    return value;
}

static int16_t bounced_velocity(int16_t velocity, uint8_t factor)
{
    uint16_t speed = (uint16_t)physics_abs_i16(velocity);

    speed = (uint16_t)((speed * factor) >> FIXED_SHIFT);
    if (speed <= BALL_STOP_SPEED) {
        return 0;
    }

    if (velocity < 0) {
        return (int16_t)speed;
    }

    return (int16_t)(0 - speed);
}

int16_t physics_wall_bounced_velocity(int16_t velocity)
{
    return bounced_velocity(velocity, WALL_BOUNCE_FACTOR);
}

int16_t physics_deflector_bounced_velocity(int16_t velocity)
{
    return bounced_velocity(velocity, DEFLECTOR_BOUNCE_FACTOR);
}

static int16_t damp_velocity_with_friction(
    int16_t velocity,
    uint8_t friction,
    uint8_t friction_shift,
    uint8_t friction_rounding)
{
    uint16_t old_speed = (uint16_t)physics_abs_i16(velocity);
    uint16_t speed = old_speed;

    if (old_speed == 0) {
        return 0;
    }

    speed = (uint16_t)(((speed * friction) + friction_rounding) >> friction_shift);
    if (speed >= old_speed) {
        speed = (uint16_t)(old_speed - 1);
    }

    if (speed <= BALL_STOP_SPEED) {
        return 0;
    }

    if (velocity < 0) {
        return (int16_t)(0 - speed);
    }

    return (int16_t)speed;
}

int16_t physics_damped_velocity(int16_t velocity)
{
    return damp_velocity_with_friction(
        velocity,
        BALL_FRICTION,
        BALL_FRICTION_SHIFT,
        BALL_FRICTION_ROUNDING);
}

int16_t physics_damped_air_velocity(int16_t velocity)
{
    return damp_velocity_with_friction(
        velocity,
        RAMP_AIR_FRICTION,
        RAMP_AIR_FRICTION_SHIFT,
        RAMP_AIR_FRICTION_ROUNDING);
}

uint8_t physics_speed_u8(int16_t vx, int16_t vy)
{
    uint16_t speed = (uint16_t)physics_abs_i16(vx);
    uint16_t speed_y = (uint16_t)physics_abs_i16(vy);

    if (speed_y > speed) {
        speed = speed_y;
    }

    if (speed > 255) {
        return 255;
    }

    return (uint8_t)speed;
}

int16_t physics_limited_motion_step(int16_t remaining)
{
    if (remaining > BALL_MAX_STEP_FIXED) {
        return BALL_MAX_STEP_FIXED;
    }

    if (remaining < -BALL_MAX_STEP_FIXED) {
        return -BALL_MAX_STEP_FIXED;
    }

    return remaining;
}

uint16_t physics_add_velocity_clamped(uint16_t position, int16_t velocity, uint16_t max_position)
{
    int16_t next = (int16_t)position + velocity;

    if (next < BALL_MIN_FIXED) {
        return BALL_MIN_FIXED;
    }

    if ((uint16_t)next > max_position) {
        return max_position;
    }

    return (uint16_t)next;
}
