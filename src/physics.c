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

static uint8_t drag_remainder_x = 0;
static uint8_t drag_remainder_y = 0;
static uint8_t drag_airborne = 0;

void physics_reset_damping(void)
{
    drag_remainder_x = 0;
    drag_remainder_y = 0;
}

static int16_t decay_with_remainder(int16_t velocity, uint8_t shift, uint8_t* remainder)
{
    uint16_t speed = (uint16_t)physics_abs_i16(velocity);
    uint16_t total = (uint16_t)(speed + *remainder);
    uint16_t loss = total >> shift;
    *remainder = (uint8_t)(total & ((1u << shift) - 1));
    speed = (uint16_t)(speed - loss);
    return velocity < 0 ? (int16_t)(0 - speed) : (int16_t)speed;
}

void physics_damp_vector(int16_t* vx, int16_t* vy, uint8_t airborne)
{
    uint8_t shift = airborne ? RAMP_AIR_FRICTION_SHIFT : BALL_FRICTION_SHIFT;
    uint16_t x;
    uint16_t y;
    if (airborne != drag_airborne) {
        physics_reset_damping();
        drag_airborne = airborne;
    }
    *vx = decay_with_remainder(*vx, shift, &drag_remainder_x);
    *vy = decay_with_remainder(*vy, shift, &drag_remainder_y);
    x = (uint16_t)physics_abs_i16(*vx);
    y = (uint16_t)physics_abs_i16(*vy);
    if (!airborne && x <= BALL_STOP_SPEED && y <= BALL_STOP_SPEED &&
        x * x + y * y <= BALL_STOP_SPEED * BALL_STOP_SPEED) {
        *vx = 0;
        *vy = 0;
        physics_reset_damping();
    }
}

uint8_t physics_speed_u8(int16_t vx, int16_t vy)
{
    uint16_t speed = (uint16_t)physics_abs_i16(vx);
    uint16_t speed_y = (uint16_t)physics_abs_i16(vy);

    if (speed_y > speed) {
        speed = speed_y;
    }

    speed >>= VELOCITY_TO_POSITION_SHIFT;
    if (speed > 255) {
        return 255;
    }

    return (uint8_t)speed;
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
