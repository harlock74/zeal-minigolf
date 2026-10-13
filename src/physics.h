#ifndef ZGOLF_PHYSICS_H
#define ZGOLF_PHYSICS_H

#include <stdint.h>

int16_t physics_abs_i16(int16_t value);
int16_t physics_wall_bounced_velocity(int16_t velocity);
int16_t physics_deflector_bounced_velocity(int16_t velocity);
void physics_reset_damping(void);
void physics_damp_vector(int16_t* vx, int16_t* vy, uint8_t airborne);
uint8_t physics_speed_u8(int16_t vx, int16_t vy);
uint16_t physics_add_velocity_clamped(uint16_t position, int16_t velocity, uint16_t max_position);

#endif
