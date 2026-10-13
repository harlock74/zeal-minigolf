#ifndef ZGOLF_SPLASH_H
#define ZGOLF_SPLASH_H
#include <stdint.h>
#include <zvb_gfx.h>

#define SPLASH_FONT_FIRST 74
#define SPLASH_SCENE_FIRST 114
#define SPLASH_SMALL_FONT 131
#define SPLASH_SMALL_DIGITS 157
#define SPLASH_TILE_COUNT 93

gfx_error splash_load_tiles(gfx_context* ctx);
void splash_render(gfx_context* ctx);
/* Returns true after a fresh Start press and the short launch cue. */
uint8_t splash_update(gfx_context* ctx, uint16_t held, uint16_t pressed);
#endif
