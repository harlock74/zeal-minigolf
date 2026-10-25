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
static uint8_t brake_valid = 0;
static uint8_t brake_x = 0;
static uint8_t brake_y = 0;
static uint8_t brake_remainder_x = 0;
static uint8_t brake_remainder_y = 0;

/* Unit-vector magnitudes in Q7, indexed by minor/major component in 1/64
 * steps. Compute once per shot/contact, not with a square root each update.
 * Fractional losses prevent tiny components from turning into axial putts. */
static const uint8_t brake_major[65] = {
    128,128,128,128,128,128,127,127,127,127,126,126,126,125,125,125,
    124,124,123,123,122,122,121,120,120,119,119,118,117,117,116,115,
    114,114,113,112,112,111,110,109,109,108,107,106,105,105,104,103,
    102,102,101,100,99,99,98,97,96,96,95,94,93,93,92,91,91
};
static const uint8_t brake_minor[65] = {
    0,2,4,6,8,10,12,14,16,18,20,22,24,25,27,29,
    31,33,35,36,38,40,42,43,45,47,48,50,51,53,54,56,
    57,59,60,61,63,64,65,67,68,69,70,71,73,74,75,76,
    77,78,79,80,81,82,83,83,84,85,86,87,88,88,89,90,91
};

void physics_reset_damping(void)
{
    drag_remainder_x = 0;
    drag_remainder_y = 0;
    brake_valid = 0;
    brake_remainder_x = 0;
    brake_remainder_y = 0;
}

static uint8_t braking_ratio(uint16_t minor, uint16_t major)
{
    uint8_t ratio = 0;
    uint8_t bits = 6;
    if (minor == major) {
        return 64;
    }
    /* Six fractional quotient bits, without SDCC's 16-bit division helper.
     * Remainder stays below the launch-speed bound before each doubling. */
    do {
        minor <<= 1;
        ratio <<= 1;
        if (minor >= major) {
            minor -= major;
            ratio++;
        }
    } while (--bits);
    return ratio;
}

static void prepare_braking(uint16_t x, uint16_t y)
{
    uint8_t ratio;
    if (x == 0 && y == 0) {
        brake_x = brake_y = 0;
    } else if (x >= y) {
        ratio = braking_ratio(y, x);
        brake_x = brake_major[ratio];
        brake_y = brake_minor[ratio];
    } else {
        ratio = braking_ratio(x, y);
        brake_x = brake_minor[ratio];
        brake_y = brake_major[ratio];
    }
    brake_valid = 1;
}

static int16_t brake_component(int16_t velocity, uint8_t unit, uint8_t* remainder)
{
    uint16_t speed = (uint16_t)physics_abs_i16(velocity);
    uint16_t total = (uint16_t)(unit * BALL_GROUND_DECEL_Q8 + *remainder);
    uint16_t loss = total >> 7;
    *remainder = (uint8_t)(total & 127);
    if (loss >= speed) {
        return 0;
    }
    speed -= loss;
    return velocity < 0 ? (int16_t)(0 - speed) : (int16_t)speed;
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
    uint8_t shift = airborne ? RAMP_AIR_FRICTION_SHIFT : BALL_GROUND_DRAG_SHIFT;
    uint16_t x;
    uint16_t y;
    if (airborne != drag_airborne) {
        physics_reset_damping();
        drag_airborne = airborne;
    }
    if (!airborne && !brake_valid) {
        prepare_braking((uint16_t)physics_abs_i16(*vx), (uint16_t)physics_abs_i16(*vy));
    }
    *vx = decay_with_remainder(*vx, shift, &drag_remainder_x);
    *vy = decay_with_remainder(*vy, shift, &drag_remainder_y);
    if (!airborne) {
        *vx = brake_component(*vx, brake_x, &brake_remainder_x);
        *vy = brake_component(*vy, brake_y, &brake_remainder_y);
    }
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
