#include <stdint.h>

#include <core.h>
#include <zos_errors.h>
#include <zos_sys.h>
#include <zos_video.h>
#include <zgdk/input.h>
#include <zvb_gfx.h>
#include <zgdk.h>
#include <zgdk/tilemap/scroll.h>

#include "aim_input.h"
#include "assets.h"
#include "config.h"
#include "course.h"
#include "hud.h"
#include "input.h"
#include "main.h"
#include "objects.h"

gfx_context vctx;
static uint16_t input_prev = 0;
static aim_input_state_t aim_input_state;
static uint8_t charging_shot = 0;
static int16_t total_score = 0;

static void render_hud(void)
{
    const course_info_t* course = course_get_info();

    hud_render_values(&vctx,
        (uint8_t)(course_get_index() + 1),
        course->par,
        objects_get_strokes(),
        total_score);
}

static void load_course(uint8_t index)
{
    const course_info_t* course;

    course_load(&vctx, index);
    course = course_get_info();
    objects_reset_ball_to_tile(course->ball_x, course->ball_y);
    objects_reset_strokes();
    hud_render_static(&vctx);
    hud_render_power_bar(&vctx, 0, 0);
    hud_render_water_message(&vctx, 0);
    render_hud();
    charging_shot = 0;
}

int main(void)
{
    init();

    while (1) {
        uint16_t input = zgolf_input_read_current();
        uint8_t rotate_left = 0;
        uint8_t rotate_right = 0;

        input = aim_input_filter(&aim_input_state, input, objects_ball_is_rolling());

        if ((input & EXIT_BUTTON_MASK) && !(input_prev & EXIT_BUTTON_MASK)) {
            deinit();
            exit(0);
        }

        aim_input_tick(&aim_input_state, input);
        rotate_left = aim_input_should_rotate_left(
            &aim_input_state,
            input,
            input_prev,
            objects_ball_is_rolling());
        rotate_right = aim_input_should_rotate_right(
            &aim_input_state,
            input,
            input_prev,
            objects_ball_is_rolling());

        if (rotate_left) {
            objects_aim_left();
            aim_input_note_rotation(&aim_input_state, BUTTON_LEFT);
        }

        if (rotate_right) {
            objects_aim_right();
            aim_input_note_rotation(&aim_input_state, BUTTON_RIGHT);
        }

        if (!objects_ball_is_rolling() &&
            (input & SHOOT_BUTTON_MASK) &&
            !(input_prev & SHOOT_BUTTON_MASK)) {
            charging_shot = 1;
            objects_begin_charge();
            input = aim_input_clear(&aim_input_state, input, &input_prev);
        }

        if ((input & SHOOT_BUTTON_MASK) && charging_shot) {
            objects_charge_shot();
        }

        if (!(input & SHOOT_BUTTON_MASK) && charging_shot) {
            charging_shot = 0;
            objects_release_shot();
            input = aim_input_clear(&aim_input_state, input, &input_prev);
        }

        objects_update_ball();
        if (objects_hole_is_complete()) {
            const course_info_t* course = course_get_info();
            uint8_t next_course = (uint8_t)(course_get_index() + 1);

            total_score = (int16_t)(total_score + ((int16_t)objects_get_strokes() - course->par));

            if (next_course >= course_get_count()) {
                next_course = 0;
                total_score = 0;
            }

            load_course(next_course);
        }

        input_prev = input;
        hud_render_power_bar(&vctx, charging_shot, objects_get_power_segments());
        hud_render_water_message(&vctx, objects_water_was_hit());
        render_hud();
        objects_render(&vctx);
    }
}

void handle_error(zos_err_t err, const char* message, uint8_t fatal)
{
    if (err != ERR_SUCCESS) {
        if (fatal) {
            deinit();
        }

        put_s("\nError[");
        put_u8((uint8_t)err);
        put_s("] ");
        put_s(message);

        if (fatal) {
            exit(err);
        }
    }
}

void init(void)
{
    zos_err_t err;
    gfx_tileset_options tileset_options = {
        .compression = TILESET_COMP_4BIT,
    };

    gfx_enable_screen(0);

    err = zgolf_input_init();
    handle_error(err, "failed to init input", 1);
    input_prev = zgolf_input_read_current();

    err = gfx_initialize(SCREEN_MODE, &vctx);
    handle_error(err, "failed to init graphics", 1);

    err = assets_load_palette(&vctx);
    handle_error(err, "failed to load palette", 1);

    err = assets_load_tileset(&vctx, &tileset_options);
    handle_error(err, "failed to load tileset", 1);

    err = gfx_tileset_add_color_tile(&vctx, EMPTY_TILE, 0);
    handle_error(err, "failed to create empty tile", 1);

    hud_init();

    tilemap_fill(&vctx, COURSE_LAYER, EMPTY_TILE, 0, 0, SCREEN_TILES_W, SCREEN_TILES_H);
    tilemap_fill(&vctx, UI_LAYER, EMPTY_TILE, 0, 0, SCREEN_TILES_W, SCREEN_TILES_H);

    course_load(&vctx, 0);

    err = objects_init();
    handle_error(err, "failed to init objects", 1);

    hud_render_static(&vctx);
    render_hud();
    objects_render(&vctx);

    gfx_enable_screen(1);
}

void deinit(void)
{
    tilemap_scroll(COURSE_LAYER, 0, 0);
    tilemap_scroll(UI_LAYER, 0, 0);
    ioctl(DEV_STDOUT, CMD_RESET_SCREEN, NULL);
}
