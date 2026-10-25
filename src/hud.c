#include <stdint.h>

#include <zgdk.h>
#include <zvb_gfx.h>

#include "config.h"
#include "course.h"
#include "hud.h"

static char hud_number[HUD_NUMBER_WIDTH];
static char hud_score[HUD_SCORE_WIDTH];
static uint8_t values_valid = 0;
static uint8_t previous_course = 0;
static uint8_t previous_par = 0;
static uint16_t previous_strokes = 0;
static int16_t previous_score = 0;
static uint8_t previous_power = 255;
static uint8_t previous_last = 255;
static uint8_t previous_water = 255;

static void clear_hud(gfx_context* ctx)
{
    uint8_t row[SCREEN_TILES_W];
    uint8_t x, y;
    for (x = 0; x < SCREEN_TILES_W; x++) row[x] = EMPTY_TILE;
    for (y = HUD_START_ROW; y < SCREEN_TILES_H; y++)
        gfx_tilemap_load(ctx, row, SCREEN_TILES_W, UI_LAYER, 0, y);
}

static void format_two_digits(uint8_t value)
{
    uint8_t tens = 0;
    if (value > 99) {
        value = 99;
    }

    /* Avoid two signed 16-bit software divisions for a two-digit value.
     * At most nine cheap byte subtractions, including on release/score HUDs. */
    while (value >= 10) {
        value -= 10;
        tens++;
    }
    hud_number[0] = (char)('0' + tens);
    hud_number[1] = (char)('0' + value);
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
    ascii_map('/', 1, TILE_ID_FONT_SLASH);
}

void hud_render_static(gfx_context* ctx)
{
    uint8_t i;
    uint8_t name_length = 0;
    const char* name = course_get_info()->name;
    clear_hud(ctx);
    values_valid = 0;
    previous_power = 255;
    previous_last = 255;
    previous_water = 255;
    while (name[name_length] != 0 && name_length < 20) {
        name_length++;
    }
    nprint_string(ctx, "                    ", 20, 2, 24);
    nprint_string(ctx, name, name_length, 2, 24);
    for (i = 25; i < SCREEN_TILES_H; i++) {
        nprint_string(ctx, "              ", 14, 26, i);
    }
    nprint_string(ctx, "COURSE", 6, HUD_COURSE_LABEL_X, HUD_COURSE_LABEL_Y);
    nprint_string(ctx, "PAR", 3, HUD_PAR_LABEL_X, HUD_PAR_LABEL_Y);
    nprint_string(ctx, "STROKE", 6, HUD_STROKE_LABEL_X, HUD_STROKE_LABEL_Y);
    nprint_string(ctx, "SCORE", 5, HUD_SCORE_LABEL_X, HUD_SCORE_LABEL_Y);
    nprint_string(ctx, "POWER", 5, HUD_POWER_LABEL_X, HUD_POWER_LABEL_Y);

    for (i = 0; i < POWER_BAR_SEGMENTS; i++) {
        gfx_tilemap_place(ctx, EMPTY_TILE, UI_LAYER, (uint8_t)(POWER_BAR_X + i), POWER_BAR_MARKER_Y);
    }
    nprint_string(ctx,"LAST",4,16,28);
    nprint_string(ctx,"HOLD Z OR B",11,15,29);
}

void hud_render_values(gfx_context* ctx, uint8_t course, uint8_t par, uint16_t strokes, int16_t score)
{
    if (!values_valid || course != previous_course) {
        format_two_digits(course);
        nprint_string(ctx, hud_number, HUD_NUMBER_WIDTH, HUD_COURSE_VALUE_X, HUD_COURSE_LABEL_Y);
        previous_course = course;
    }
    if (!values_valid || par != previous_par) {
        format_two_digits(par);
        nprint_string(ctx, hud_number, HUD_NUMBER_WIDTH, HUD_PAR_VALUE_X, HUD_PAR_LABEL_Y);
        previous_par = par;
    }
    if (!values_valid || strokes != previous_strokes) {
        format_two_digits(strokes > 99 ? 99 : (uint8_t)strokes);
        nprint_string(ctx, hud_number, HUD_NUMBER_WIDTH, HUD_STROKE_VALUE_X, HUD_STROKE_LABEL_Y);
        previous_strokes = strokes;
    }
    if (!values_valid || score != previous_score) {
        format_golf_score(score);
        nprint_string(ctx, hud_score, HUD_SCORE_WIDTH, HUD_SCORE_VALUE_X, HUD_SCORE_LABEL_Y);
        previous_score = score;
    }
    values_valid = 1;
}

void hud_render_power_bar(gfx_context* ctx, uint8_t visible, uint8_t level, uint8_t last)
{
    uint8_t i;
    uint8_t row[POWER_BAR_SEGMENTS];
    if (!visible) level=0;
    if(level>SHOT_POWER_LEVELS) level=SHOT_POWER_LEVELS;
    if(last>SHOT_POWER_LEVELS) last=SHOT_POWER_LEVELS;
    if(level!=previous_power) {
        uint8_t first=POWER_BAR_SEGMENTS;
        uint8_t end=0;
        for(i=0;i<POWER_BAR_SEGMENTS;i++) {
            uint8_t fill=level>i*4 ? level-i*4 : 0;
            uint8_t old=previous_power>i*4 ? previous_power-i*4 : 0;
            if(fill>4) fill=4;
            if(old>4) old=4;
            row[i]=TILE_ID_POWER_TRACK+fill;
            if(previous_power==255 || fill!=old) {
                if(first==POWER_BAR_SEGMENTS) first=i;
                end=i+1;
            }
        }
        if(end>first)
            gfx_tilemap_load(ctx,row+first,end-first,UI_LAYER,POWER_BAR_X+first,POWER_BAR_Y);
        if(level==32) nprint_string(ctx,"FULL ",5,21,25);
        else {
            format_two_digits(level);
            if(level) nprint_string(ctx,hud_number,2,21,25);
            else nprint_string(ctx,"--/32",5,21,25);
            if(level) nprint_string(ctx,"/32",3,23,25);
        }
        nprint_string(ctx,visible ? "X OR A STOP" : "HOLD Z OR B",11,15,29);
        previous_power=level;
    }
    if(last!=previous_last) {
        for(i=0;i<POWER_BAR_SEGMENTS;i++) {
            uint8_t tile=last && (last-1)/4==i ? TILE_ID_POWER_LAST+(last-1)%4 : EMPTY_TILE;
            row[i]=tile;
        }
        gfx_tilemap_load(ctx,row,POWER_BAR_SEGMENTS,UI_LAYER,POWER_BAR_X,POWER_BAR_MARKER_Y);
        format_two_digits(last);
        if(last) nprint_string(ctx,hud_number,2,22,28);
        else nprint_string(ctx,"--",2,22,28);
        previous_last=last;
    }
}

void hud_render_water_message(gfx_context* ctx, uint8_t visible)
{
    if (visible == previous_water) {
        return;
    }
    previous_water = visible;
    if (visible) {
        nprint_string(ctx, "BALL IN", HUD_WATER_LINE_1_WIDTH, HUD_WATER_LINE_1_X, HUD_WATER_LINE_1_Y);
        nprint_string(ctx, "THE WATER", HUD_WATER_LINE_2_WIDTH, HUD_WATER_LINE_2_X, HUD_WATER_LINE_2_Y);
    } else {
        nprint_string(ctx, "       ", HUD_WATER_LINE_1_WIDTH, HUD_WATER_LINE_1_X, HUD_WATER_LINE_1_Y);
        nprint_string(ctx, "         ", HUD_WATER_LINE_2_WIDTH, HUD_WATER_LINE_2_X, HUD_WATER_LINE_2_Y);
    }
}

void hud_render_result(gfx_context* ctx, uint8_t round_complete)
{
    const char* name = course_get_info()->name;
    uint8_t length = 0;
    /* Values are the completed-hole snapshot from hud_render_values(). */
    clear_hud(ctx);
    while (length < 20 && name[length]) length++;
    nprint_string(ctx, name, length, 2, 24);
    nprint_string(ctx, "HOLE", 4, 30, 24);
    format_two_digits(previous_course);
    nprint_string(ctx, hud_number, 2, 35, 24);
    nprint_string(ctx, "STROKES", 7, 2, 27);
    format_two_digits(previous_strokes > 99 ? 99 : (uint8_t)previous_strokes);
    nprint_string(ctx, hud_number, 2, 10, 27);
    nprint_string(ctx, "PAR", 3, 16, 27);
    format_two_digits(previous_par);
    nprint_string(ctx, hud_number, 2, 20, 27);
    nprint_string(ctx, "TOTAL", 5, 26, 27);
    format_golf_score(previous_score);
    nprint_string(ctx, hud_score, HUD_SCORE_WIDTH, 32, 27);
    if (round_complete) {
        nprint_string(ctx, "ROUND COMPLETE", 14, 13, 25);
        nprint_string(ctx, "SPACE Z OR B TO REPLAY", 22, 9, 29);
    } else {
        nprint_string(ctx, "HOLE COMPLETE", 13, 13, 25);
        nprint_string(ctx, "SPACE Z OR B TO CONTINUE", 24, 8, 29);
    }
}
