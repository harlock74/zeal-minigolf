#ifndef ZGOLF_SPLASH_H
#define ZGOLF_SPLASH_H
#include <stdint.h>
#include <zvb_gfx.h>

void splash_render(gfx_context* ctx);
/* Returns true after a fresh Start press and the short launch cue. */
uint8_t splash_update(gfx_context* ctx, uint16_t held, uint16_t pressed);
#endif
