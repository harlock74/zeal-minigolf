#ifndef ZGOLF_RAMP_H
#define ZGOLF_RAMP_H

#include <stdint.h>

void ramp_reset(void);
void ramp_update(void);
uint8_t ramp_is_airborne(void);
uint8_t ramp_can_start(void);
uint8_t ramp_tile_is(uint8_t tile);
uint8_t ramp_accepts_direction(uint8_t tile, int16_t vx, int16_t vy);
uint8_t ramp_arc_offset(void);
void ramp_start_jump(uint8_t speed, int16_t* vx, int16_t* vy);
void ramp_start_failed_rebound(void);
int16_t ramp_failed_rebound_velocity(int16_t velocity);

#endif
