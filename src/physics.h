#ifndef ZGOLF_PHYSICS_H
#define ZGOLF_PHYSICS_H

#include <stdint.h>

int16_t physics_abs_i16(int16_t value);
int16_t physics_wall_bounced_velocity(int16_t velocity);
int16_t physics_deflector_bounced_velocity(int16_t velocity);
int16_t physics_damped_velocity(int16_t velocity);
int16_t physics_damped_air_velocity(int16_t velocity);
uint8_t physics_speed_u8(int16_t vx, int16_t vy);
int16_t physics_limited_motion_step(int16_t remaining);
uint16_t physics_add_velocity_clamped(uint16_t position, int16_t velocity, uint16_t max_position);

#endif
