#include <stdint.h>
#include <zgdk/input.h>
#include <zvb_gfx.h>
#include <zvb_sprite.h>
#include "config.h"
#include "feedback.h"
#include "splash.h"
#include "splash.generated.h"

static uint8_t blink, visible, ready, starting, pause_ticks, segment;
static uint16_t segment_ticks;
static int32_t ball_x, ball_y;
static gfx_sprite logo_ball;

/* Small row buffer; the editable screen stays compressed in program storage. */
static void load_layer(gfx_context* ctx, const uint8_t* source, uint8_t layer)
{
    uint8_t row[SCREEN_TILES_W], x, y, count = 0, tile = EMPTY_TILE;
    for (y = 0; y < SCREEN_TILES_H; y++) {
        for (x = 0; x < SCREEN_TILES_W; x++) {
            if (!count) {count = *source++; tile = *source++;}
            row[x] = tile;
            count--;
        }
        gfx_tilemap_load(ctx, row, SCREEN_TILES_W, layer, 0, y);
    }
}

static void prompt(gfx_context* ctx, uint8_t launch)
{
    uint8_t row[SPLASH_PROMPT_WIDTH], i;
    for (i = 0; i < SPLASH_PROMPT_WIDTH; i++)
        row[i] = launch ? splash_launch[i] : visible ? splash_prompt_on[i] : EMPTY_TILE;
    gfx_tilemap_load(ctx, row, SPLASH_PROMPT_WIDTH, UI_LAYER, SPLASH_PROMPT_X, SPLASH_PROMPT_Y);
}

static void reset_ball(void)
{
    segment = 0;
    segment_ticks = 0;
    ball_x = (int32_t)splash_path_x[0] * 256;
    ball_y = (int32_t)splash_path_y[0] * 256;
}

void splash_render(gfx_context* ctx)
{
    uint8_t i;
    load_layer(ctx, splash_course, COURSE_LAYER);
    load_layer(ctx, splash_lettering, UI_LAYER);
    blink = ready = starting = pause_ticks = 0;
    visible = 1;
    reset_ball();
    logo_ball.x = 0; logo_ball.y = 0; logo_ball.tile = EMPTY_TILE;
    logo_ball.flags = SPRITE_NONE; logo_ball.options = SPRITE_OPTION_NONE;
    for (i = 0; i < 128; i++) gfx_sprite_render(ctx, i, &logo_ball);
    logo_ball.x = splash_path_x[0]; logo_ball.y = splash_path_y[0];
    logo_ball.tile = TILE_ID_BALL;
    gfx_sprite_render(ctx, 0, &logo_ball);
}

uint8_t splash_update(gfx_context* ctx, uint16_t held, uint16_t pressed)
{
    if (starting) return --starting == 0;
    if (!(held & BUTTON_START)) ready = 1;
    if (ready && (pressed & BUTTON_START)) {
        prompt(ctx, 1);
        feedback_play(FEEDBACK_SHOT);
        starting = 8;
        return 0;
    }
    if (++blink == SPLASH_BLINK_TICKS) {blink = 0; visible ^= 1; prompt(ctx, 0);}
    if (pause_ticks) {
        if (--pause_ticks == 0) reset_ball();
    } else {
        ball_x += splash_path_dx[segment];
        ball_y += splash_path_dy[segment];
        if (++segment_ticks == splash_path_ticks[segment]) {
            segment++;
            ball_x = (int32_t)splash_path_x[segment] * 256;
            ball_y = (int32_t)splash_path_y[segment] * 256;
            segment_ticks = 0;
            if (segment == SPLASH_PATH_SEGMENTS) pause_ticks = SPLASH_PAUSE_TICKS;
        }
    }
    logo_ball.x = (uint16_t)(ball_x / 256);
    logo_ball.y = (uint16_t)(ball_y / 256);
    logo_ball.tile = pause_ticks ? (pause_ticks > SPLASH_PAUSE_TICKS / 2 ? TILE_ID_BALL_SHADOW : EMPTY_TILE) : TILE_ID_BALL;
    gfx_sprite_render(ctx, 0, &logo_ball);
    return 0;
}
