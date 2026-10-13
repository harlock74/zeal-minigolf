#ifndef ZGOLF_HUD_H
#define ZGOLF_HUD_H

#include <stdint.h>
#include <zvb_gfx.h>

void hud_init(void);
gfx_error hud_load_power_tiles(gfx_context* ctx);
void hud_render_result(gfx_context* ctx, uint8_t round_complete);
void hud_render_static(gfx_context* ctx);
void hud_render_values(gfx_context* ctx, uint8_t course, uint8_t par, uint16_t strokes, int16_t score);
void hud_render_power_bar(gfx_context* ctx, uint8_t visible, uint8_t level, uint8_t last);
void hud_render_water_message(gfx_context* ctx, uint8_t visible);

#endif
