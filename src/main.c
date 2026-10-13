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
#include "feedback.h"
#include "hud.h"
#include "input.h"
#include "main.h"
#include "objects.h"
#include "shot.h"
#include "splash.h"

gfx_context vctx;
static uint16_t input_prev = 0;
static aim_input_state_t aim_input_state;
static shot_input_state_t shot_input;
static int16_t total_score = 0;
static uint8_t result_pending = 0;
static uint8_t result_ready = 0;

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

    feedback_suspend(0);
    gfx_enable_screen(0);
    course_load(&vctx, index);
    course = course_get_info();
    objects_reset_ball_to_tile(course->ball_x, course->ball_y);
    objects_reset_strokes();
    hud_render_static(&vctx);
    hud_render_power_bar(&vctx, 0, 0, 0);
    hud_render_water_message(&vctx, 0);
    render_hud();
    shot_input.charging = 0;
    shot_input.wait_release = 0;
    result_pending = 0;
    result_ready = 0;
    aim_input_reset(&aim_input_state);
    sprites_render(&vctx);
    gfx_enable_screen(1);
}

int main(void)
{
    init();

    while (1) {
        uint16_t input = zgolf_input_read_current();
        uint16_t pressed = zgolf_input_pressed();
        uint8_t rotate_left = 0;
        uint8_t rotate_right = 0;

        input = aim_input_filter(&aim_input_state, input, objects_ball_is_rolling());

        if (pressed & EXIT_BUTTON_MASK) {
            deinit();
            exit(0);
        }

        feedback_tick();
        if (result_pending) {
            if (!(input & SHOOT_BUTTON_MASK)) {
                result_ready = 1;
            }
            if (result_ready && (pressed & SHOOT_BUTTON_MASK)) {
                uint8_t next = (uint8_t)(course_get_index() + 1);
                if (next >= course_get_count()) {
                    next = 0;
                    total_score = 0;
                }
                load_course(next);
            }
            /* The continue press is consumed; release before charging again. */
            input_prev = input;
            objects_render(&vctx);
            continue;
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

        {
            uint8_t events=shot_input_update(&shot_input,input,pressed,zgolf_input_released(),
                (uint8_t)(!objects_ball_is_rolling()));
            if(events & SHOT_INPUT_BEGIN) objects_begin_charge();
            if(events & SHOT_INPUT_TICK) {
                uint8_t previous=shot_power_level();
                objects_charge_shot();
                if(shot_power_level()!=previous && shot_power_level()%8==0)
                    feedback_play(shot_power_level()==32 ? FEEDBACK_FULL : FEEDBACK_POWER);
            }
            if(events & SHOT_INPUT_RELEASE) objects_release_shot();
            if(events & (SHOT_INPUT_BEGIN | SHOT_INPUT_RELEASE | SHOT_INPUT_CANCEL))
                input=aim_input_clear(&aim_input_state,input,&input_prev);
        }

        objects_update_ball();
        if (objects_hole_is_complete()) {
            const course_info_t* course = course_get_info();
            total_score = (int16_t)(total_score + ((int16_t)objects_get_strokes() - course->par));
            shot_input.charging = 0;
            result_pending = 1;
            feedback_suspend(1);
            result_ready = (uint8_t)(!(input & SHOOT_BUTTON_MASK));
            hud_render_power_bar(&vctx, 0, 0, shot_last_power_level());
            hud_render_water_message(&vctx, 0);
            render_hud();
            hud_render_result(&vctx, (uint8_t)(course_get_index() + 1 >= course_get_count()));
            input_prev = input;
            objects_render(&vctx);
            continue;
        }

        input_prev = input;
        hud_render_power_bar(&vctx, shot_input.charging, shot_power_level(), shot_last_power_level());
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

    feedback_init();
    hud_init();
    err = hud_load_power_tiles(&vctx);
    handle_error(err,"failed to load power display",1);

    tilemap_fill(&vctx, COURSE_LAYER, EMPTY_TILE, 0, 0, SCREEN_TILES_W, SCREEN_TILES_H);
    tilemap_fill(&vctx, UI_LAYER, EMPTY_TILE, 0, 0, SCREEN_TILES_W, SCREEN_TILES_H);

    err = splash_load_tiles(&vctx);
    handle_error(err, "failed to load intro", 1);
    splash_render(&vctx);
    gfx_enable_screen(1);
    zgolf_input_flush();
    while (1) {
        uint16_t held = zgolf_input_read_current();
        uint16_t pressed = zgolf_input_pressed();
        uint8_t started;
        if (pressed & EXIT_BUTTON_MASK) {deinit(); exit(0);}
        feedback_tick();
        gfx_wait_vblank(&vctx);
        started = splash_update(&vctx, held, pressed);
        gfx_wait_end_vblank(&vctx);
        if (started) break;
    }
    gfx_enable_screen(0);
    tilemap_fill(&vctx, UI_LAYER, EMPTY_TILE, 0, 0, SCREEN_TILES_W, SCREEN_TILES_H);
    zgolf_input_flush();
    input_prev = zgolf_input_read_current();

    hud_init(); /* Restore the gameplay font after the transparent intro font. */
    course_load(&vctx, START_COURSE_INDEX);

    err = objects_init();
    handle_error(err, "failed to init objects", 1);

    hud_render_static(&vctx);
    hud_render_power_bar(&vctx,0,0,0);
    render_hud();
    objects_render(&vctx);

    gfx_enable_screen(1);
}

void deinit(void)
{
    feedback_stop();
    tilemap_scroll(COURSE_LAYER, 0, 0);
    tilemap_scroll(UI_LAYER, 0, 0);
    ioctl(DEV_STDOUT, CMD_RESET_SCREEN, NULL);
}
