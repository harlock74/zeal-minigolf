#include <stdint.h>
#include <zgdk.h>
#include <zgdk/input.h>
#include <zvb_gfx.h>
#include <zvb_sprite.h>
#include "assets.h"
#include "config.h"
#include "feedback.h"
#include "splash.h"

static const char letters[] = "ZEALMINGOF";
static uint8_t blink, visible, ready, starting, pause;
static uint16_t travel;
static gfx_sprite logo_ball;

static uint8_t scene_pixel(uint8_t variant, uint8_t x, uint8_t y)
{
    if (variant < 9) return variant;
    if (variant == 9) return ((x == 8 && y >= 4 && y <= 12) ||
        (y == 8 && x >= 4 && x <= 12)) ? 7 : 0;
    if (variant == 10) return (y & 7) < 2 ? 3 : 2;
    if (variant == 11) return ((x >> 3) ^ (y >> 3)) & 1 ? 5 : 4;
    if (variant == 12) {
        if (x == 3 && y >= 2 && y <= 14) return 7;
        if (y >= 2 && y <= 8 && x >= 4 && x <= 13 - (y - 2)) return 8;
        return 4;
    }
    {
        int16_t dx = (int16_t)(x + ((variant - 13) & 1) * 16) * 2 - 31;
        int16_t dy = (int16_t)(y + ((variant - 13) >> 1) * 16) * 2 - 31;
        return dx * dx + dy * dy < 900 ? 7 : 0;
    }
}

gfx_error splash_load_tiles(gfx_context* ctx)
{
    static const uint16_t palette[] = {
        0x08a4u, 0x0968u, 0x1aeeu, 0x3453u, 0x2328u,
        0x6509u, 0xd58eu, 0xf779u, 0xf3cbu
    };
    uint8_t pixels[128];
    uint8_t tile, x, y;
    gfx_error err = gfx_palette_load(ctx, (void*)palette, sizeof(palette), 16);
    if (err != GFX_SUCCESS) return err;
    for (tile = 0; tile < SPLASH_TILE_COUNT; tile++) {
        const uint8_t* source = tile < 40 ? assets_get_packed_tile(
            FONT_ALPHA_A_TILE + letters[tile >> 2] - 'A') : 0;
        if(tile>=57) source=assets_get_packed_tile(tile<83 ?
            FONT_ALPHA_A_TILE+tile-57 : FONT_DIGIT_TILE+tile-83);
        gfx_tileset_options options = {.compression = TILESET_COMP_4BIT,
            .from_byte = (uint16_t)((uint16_t)(SPLASH_FONT_FIRST + tile) << 8),
            .pal_offset = tile >= 40 && tile < 57 ? 16 : 0};
        for (y = 0; y < 16; y++) for (x = 0; x < 16; x += 2) {
            uint8_t a, b;
            if (tile < 40) {
                uint8_t q = tile & 3;
                uint8_t sx = (uint8_t)((x + (q & 1) * 16) >> 1);
                uint8_t sy = (uint8_t)((y + (q >> 1) * 16) >> 1);
                uint8_t packed = source[sy * 8 + (sx >> 1)];
                a = (sx & 1) ? packed & 15 : packed >> 4;
                /* Each source pixel becomes a horizontal pair. */
                /* The source font uses both palette 0 and 1 for black. */
                a = a > 1 ? ((y + (q >> 1) * 16) < 15 ? 10 : 11) : 0;
                b = a;
            } else if(tile>=57) {
                uint8_t packed=source[y*8+(x>>1)];
                a=packed>>4;b=packed&15;
                if(a<2) a=0;
                if(b<2) b=0;
            } else {
                a = scene_pixel(tile - 40, x, y);
                b = scene_pixel(tile - 40, x + 1, y);
            }
            pixels[y * 8 + x / 2] = (uint8_t)((a << 4) | b);
        }
        err = gfx_tileset_load(ctx, pixels, sizeof(pixels), &options);
        if (err != GFX_SUCCESS) return err;
    }
    return GFX_SUCCESS;
}

static void centered(gfx_context* ctx, const char* text, uint8_t length, uint8_t y)
{
    nprint_string(ctx, text, length, (SCREEN_TILES_W - length) / 2, y);
}

static void prompt(gfx_context* ctx)
{
    centered(ctx, visible ? "PRESS START TO PLAY" : "                   ", 19, 16);
}

static uint8_t background(uint8_t x, uint8_t y)
{
    int16_t dx = (int16_t)x - 20;
    int16_t dy = (int16_t)y - 23;
    uint8_t tile = SPLASH_SCENE_FIRST;
    if (y >= 8 && y <= 10 && (x < 11 || x > 28)) tile++;
    if (y >= 18 && dx * dx + dy * dy * 9 < 340) tile = SPLASH_SCENE_FIRST + 10;
    if (y >= 20 && y <= 26) {
        uint8_t distance = (uint8_t)(dy < 0 ? -dy : dy);
        uint8_t inset = (uint8_t)(5 + distance * 2);
        if (x >= inset - 1 && x <= 40 - inset) {
            tile = SPLASH_SCENE_FIRST + 6;
            if (y >= 21 && y <= 25 && x >= inset && x < 40 - inset)
                tile = SPLASH_SCENE_FIRST + 11;
            if (x >= 20 && x <= 22) tile = SPLASH_SCENE_FIRST + 10;
        }
    }
    return tile;
}

void splash_render(gfx_context* ctx)
{
    static const char title[] = "ZEAL MINIGOLF";
    uint8_t row[40], x, y, i;
    ascii_map('A',26,SPLASH_SMALL_FONT);
    ascii_map('0',10,SPLASH_SMALL_DIGITS);
    for (y = 0; y < 30; y++) {
        for (x = 0; x < 40; x++) row[x] = background(x, y);
        gfx_tilemap_load(ctx, row, 40, COURSE_LAYER, 0, y);
        for (x = 0; x < 40; x++) row[x] = EMPTY_TILE;
        gfx_tilemap_load(ctx, row, 40, UI_LAYER, 0, y);
    }
    for (i = 0; i < 13; i++) if (title[i] != ' ') {
        uint8_t glyph = 0, q;
        while (letters[glyph] != title[i]) glyph++;
        for (q = 0; q < 4; q++) gfx_tilemap_place(ctx,
            SPLASH_FONT_FIRST + glyph * 4 + q, UI_LAYER,
            7 + i * 2 + (q & 1), 12 + (q >> 1));
    }
    for (i = 0; i < 4; i++) gfx_tilemap_place(ctx,
        SPLASH_SCENE_FIRST + 13 + i, COURSE_LAYER, 32 + (i & 1), 4 + (i >> 1));
    gfx_tilemap_place(ctx, SPLASH_SCENE_FIRST + 9, COURSE_LAYER, 6, 4);
    gfx_tilemap_place(ctx, SPLASH_SCENE_FIRST + 9, COURSE_LAYER, 13, 6);
    gfx_tilemap_place(ctx, SPLASH_SCENE_FIRST + 9, COURSE_LAYER, 24, 3);
    gfx_tilemap_place(ctx, SPLASH_SCENE_FIRST + 12, COURSE_LAYER, 31, 22);
    gfx_tilemap_place(ctx, TILE_ID_HOLE, COURSE_LAYER, 31, 23);
    centered(ctx, "A LITTLE GOLF AFTER DARK", 24, 8);
    centered(ctx, "ENTER ON KEYBOARD", 17, 17);
    centered(ctx, "15 COURSES  ONE PERFECT SHOT", 28, 28);
    blink = ready = starting = pause = 0;
    travel = 0; visible = 1;
    logo_ball.x = 0; logo_ball.y = 0; logo_ball.tile = EMPTY_TILE;
    logo_ball.flags = SPRITE_NONE; logo_ball.options = SPRITE_OPTION_NONE;
    /* Do not inherit sprites from the previously launched game. */
    for(i=0;i<128;i++) gfx_sprite_render(ctx,i,&logo_ball);
    logo_ball.x = 8*16+16; logo_ball.y = 23*16+16; logo_ball.tile = TILE_ID_BALL;
    prompt(ctx);
    gfx_sprite_render(ctx,0,&logo_ball);
}

uint8_t splash_update(gfx_context* ctx, uint16_t held, uint16_t pressed)
{
    if (starting) return --starting == 0;
    if (!(held & BUTTON_START)) ready = 1;
    if (ready && (pressed & BUTTON_START)) {
        centered(ctx, "                   ", 19, 16);
        centered(ctx, "LET US PLAY", 11, 16);
        feedback_play(FEEDBACK_SHOT);
        starting = 8;
        return 0;
    }
    if (++blink == 36) {blink = 0; visible ^= 1; prompt(ctx);}
    if(pause) {
        if(--pause==0) travel=0;
    } else {
        travel+=3;
        if(travel>=368) {travel=368;pause=24;}
    }
    logo_ball.x = (uint16_t)(8*16+16+travel);
    logo_ball.y = 23*16+16;
    logo_ball.tile = pause ? (pause>12 ? TILE_ID_BALL_SHADOW : EMPTY_TILE) : TILE_ID_BALL;
    if(travel>=176 && travel<=264)
        logo_ball.y -= travel<220 ? travel-176 : 264-travel;
    gfx_sprite_render(ctx,0,&logo_ball);
    return 0;
}
