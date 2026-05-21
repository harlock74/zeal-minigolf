#include <stdint.h>

#include <zgdk.h>
#include <zvb_gfx.h>

#include "config.h"
#include "hud.h"

static char hud_number[HUD_NUMBER_WIDTH];
static char hud_score[HUD_SCORE_WIDTH];

static void format_two_digits(uint8_t value)
{
    if (value > 99) {
        value = 99;
    }

    hud_number[0] = (char)('0' + (value / 10));
    hud_number[1] = (char)('0' + (value % 10));
}

static void format_golf_score(int16_t score)
{
    uint8_t value;

    hud_score[2] = ' ';

    if (score == 0) {
        hud_score[0] = 'E';
        hud_score[1] = ' ';
        return;
    }

    if (score < 0) {
        hud_score[0] = '-';
        score = (int16_t)(0 - score);
    } else {
        hud_score[0] = '+';
    }

    if (score > 99) {
        score = 99;
    }

    value = (uint8_t)score;
    if (value > 9) {
        hud_score[1] = (char)('0' + (value / 10));
        hud_score[2] = (char)('0' + (value % 10));
    } else {
        hud_score[1] = (char)('0' + value);
    }
}

void hud_init(void)
{
    ascii_map(' ', 1, EMPTY_TILE);
    ascii_map('0', 10, FONT_DIGIT_TILE);
    ascii_map('A', 26, FONT_ALPHA_A_TILE);
    ascii_map('a', 26, FONT_ALPHA_A_TILE);
    ascii_map('+', 1, TILE_ID_SCORE_PLUS);
    ascii_map('-', 1, TILE_ID_SCORE_MINUS);
}

void hud_render_static(gfx_context* ctx)
{
    uint8_t i;

    nprint_string(ctx, "COURSE", 6, HUD_COURSE_LABEL_X, HUD_COURSE_LABEL_Y);
    nprint_string(ctx, "PAR", 3, HUD_PAR_LABEL_X, HUD_PAR_LABEL_Y);
    nprint_string(ctx, "STROKE", 6, HUD_STROKE_LABEL_X, HUD_STROKE_LABEL_Y);
    nprint_string(ctx, "SCORE", 5, HUD_SCORE_LABEL_X, HUD_SCORE_LABEL_Y);
    nprint_string(ctx, "POWER", 5, HUD_POWER_LABEL_X, HUD_POWER_LABEL_Y);

    for (i = 0; i < POWER_BAR_SEGMENTS; i++) {
        gfx_tilemap_place(ctx, TILE_ID_POWER_MARKER, UI_LAYER, (uint8_t)(POWER_BAR_X + i), POWER_BAR_MARKER_Y);
    }
}

void hud_render_values(gfx_context* ctx, uint8_t course, uint8_t par, uint16_t strokes, int16_t score)
{
    format_two_digits(course);
    nprint_string(ctx, hud_number, HUD_NUMBER_WIDTH, HUD_COURSE_VALUE_X, HUD_COURSE_LABEL_Y);

    format_two_digits(par);
    nprint_string(ctx, hud_number, HUD_NUMBER_WIDTH, HUD_PAR_VALUE_X, HUD_PAR_LABEL_Y);

    format_two_digits((uint8_t)strokes);
    nprint_string(ctx, hud_number, HUD_NUMBER_WIDTH, HUD_STROKE_VALUE_X, HUD_STROKE_LABEL_Y);

    format_golf_score(score);
    nprint_string(ctx, hud_score, HUD_SCORE_WIDTH, HUD_SCORE_VALUE_X, HUD_SCORE_LABEL_Y);
}

void hud_render_power_bar(gfx_context* ctx, uint8_t visible, uint8_t filled)
{
    uint8_t i;

    if (!visible) {
        filled = 0;
    }

    for (i = 0; i < POWER_BAR_SEGMENTS; i++) {
        uint8_t tile = EMPTY_TILE;

        if (i < filled) {
            tile = TILE_ID_POWER;
        }

        gfx_tilemap_place(ctx, tile, UI_LAYER, (uint8_t)(POWER_BAR_X + i), POWER_BAR_Y);
    }
}

void hud_render_water_message(gfx_context* ctx, uint8_t visible)
{
    if (visible) {
        nprint_string(ctx, "BALL IN", HUD_WATER_LINE_1_WIDTH, HUD_WATER_LINE_1_X, HUD_WATER_LINE_1_Y);
        nprint_string(ctx, "THE WATER", HUD_WATER_LINE_2_WIDTH, HUD_WATER_LINE_2_X, HUD_WATER_LINE_2_Y);
    } else {
        nprint_string(ctx, "       ", HUD_WATER_LINE_1_WIDTH, HUD_WATER_LINE_1_X, HUD_WATER_LINE_1_Y);
        nprint_string(ctx, "         ", HUD_WATER_LINE_2_WIDTH, HUD_WATER_LINE_2_X, HUD_WATER_LINE_2_Y);
    }
}
