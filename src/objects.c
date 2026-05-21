#include <stdint.h>

#include <zgdk.h>
#include <zvb_gfx.h>
#include <zvb_sprite.h>

#include "aim.h"
#include "config.h"
#include "coords.h"
#include "course.h"
#include "objects.h"
#include "physics.h"
#include "ramp.h"
#include "shot.h"
#include "terrain.h"

static gfx_sprite sprite_arena[SPRITE_ARENA_SIZE];
static gfx_sprite* ball_sprite;
static gfx_sprite* ball_shadow_sprite;
static gfx_sprite* aim_markers[AIM_MARKER_COUNT];
static uint8_t hole_complete = 0;
static uint8_t water_hit = 0;
static uint16_t last_safe_x = 0;
static uint16_t last_safe_y = 0;

typedef struct {
    uint16_t x;
    uint16_t y;
    int16_t vx;
    int16_t vy;
} ball_state_t;

static ball_state_t ball;

static uint8_t ball_tile_at(uint16_t x, uint16_t y)
{
    return course_get_tile(coords_fixed_to_center_tile(x), coords_fixed_to_center_tile(y));
}

static uint8_t ball_sample_outside_playfield(uint16_t x, uint16_t y)
{
    uint8_t tile_x = coords_fixed_to_center_tile(x);
    uint8_t tile_y = coords_fixed_to_center_tile(y);

    return (uint8_t)(tile_x >= COURSE_TILES_W || tile_y >= PLAYFIELD_TILES_H);
}

static uint8_t ball_is_airborne(void)
{
    return ramp_is_airborne();
}

static uint8_t deflector_solid_at(uint8_t tile, uint16_t x, uint16_t y)
{
    uint8_t local_x = (uint8_t)(coords_fixed_to_center_pixel(x) & TILE_PIXEL_MASK);
    uint8_t local_y = (uint8_t)(coords_fixed_to_center_pixel(y) & TILE_PIXEL_MASK);
    int16_t diagonal_y = local_x;
    int16_t distance;

    if (terrain_tile_is_slash_deflector(tile)) {
        diagonal_y = (int16_t)(TILE_PIXEL_MASK - local_x);
    }

    distance = (int16_t)local_y - diagonal_y;
    if (distance < 0) {
        distance = (int16_t)(0 - distance);
    }

    return (uint8_t)(distance <= DEFLECTOR_SOLID_HALF_WIDTH_PIXELS);
}

static uint8_t ball_tile_at_offset(uint16_t x, uint16_t y, int8_t offset_x, int8_t offset_y)
{
    int16_t sample_x = (int16_t)x + ((int16_t)offset_x << FIXED_SHIFT);
    int16_t sample_y = (int16_t)y + ((int16_t)offset_y << FIXED_SHIFT);

    if (sample_x < BALL_MIN_FIXED) {
        sample_x = BALL_MIN_FIXED;
    }

    if (sample_y < BALL_MIN_FIXED) {
        sample_y = BALL_MIN_FIXED;
    }

    return ball_tile_at((uint16_t)sample_x, (uint16_t)sample_y);
}

static uint8_t ball_collision_tile_at_offset(uint16_t x, uint16_t y, int8_t offset_x, int8_t offset_y)
{
    int16_t sample_x = (int16_t)x + ((int16_t)offset_x << FIXED_SHIFT);
    int16_t sample_y = (int16_t)y + ((int16_t)offset_y << FIXED_SHIFT);
    uint8_t tile;

    if (sample_x < BALL_MIN_FIXED || sample_y < BALL_MIN_FIXED) {
        return TILE_ID_WALL;
    }

    if (ball_sample_outside_playfield((uint16_t)sample_x, (uint16_t)sample_y)) {
        return TILE_ID_WALL;
    }

    tile = ball_tile_at((uint16_t)sample_x, (uint16_t)sample_y);
    if (terrain_tile_is_deflector(tile) &&
        !deflector_solid_at(tile, (uint16_t)sample_x, (uint16_t)sample_y)) {
        return EMPTY_TILE;
    }

    return tile;
}

static uint8_t ball_collision_tile_x(uint16_t x, uint16_t y, int16_t velocity)
{
    int8_t edge = BALL_COLLISION_RADIUS_PIXELS;
    uint8_t tile;

    if (velocity < 0) {
        edge = -BALL_COLLISION_RADIUS_PIXELS;
    }

    tile = ball_collision_tile_at_offset(x, y, edge, -BALL_COLLISION_RADIUS_PIXELS);
    if (tile == TILE_ID_WALL || terrain_tile_is_deflector(tile)) {
        return tile;
    }

    tile = ball_collision_tile_at_offset(x, y, edge, 0);
    if (tile == TILE_ID_WALL || terrain_tile_is_deflector(tile)) {
        return tile;
    }

    tile = ball_collision_tile_at_offset(x, y, edge, BALL_COLLISION_RADIUS_PIXELS);
    if (tile == TILE_ID_WALL || terrain_tile_is_deflector(tile)) {
        return tile;
    }

    return EMPTY_TILE;
}

static uint8_t ball_collision_tile_y(uint16_t x, uint16_t y, int16_t velocity)
{
    int8_t edge = BALL_COLLISION_RADIUS_PIXELS;
    uint8_t tile;

    if (velocity < 0) {
        edge = -BALL_COLLISION_RADIUS_PIXELS;
    }

    tile = ball_collision_tile_at_offset(x, y, -BALL_COLLISION_RADIUS_PIXELS, edge);
    if (tile == TILE_ID_WALL || terrain_tile_is_deflector(tile)) {
        return tile;
    }

    tile = ball_collision_tile_at_offset(x, y, 0, edge);
    if (tile == TILE_ID_WALL || terrain_tile_is_deflector(tile)) {
        return tile;
    }

    tile = ball_collision_tile_at_offset(x, y, BALL_COLLISION_RADIUS_PIXELS, edge);
    if (tile == TILE_ID_WALL || terrain_tile_is_deflector(tile)) {
        return tile;
    }

    return EMPTY_TILE;
}

static uint8_t ball_over_hole(void)
{
    if (ball_tile_at(ball.x, ball.y) == TILE_ID_HOLE) {
        return 1;
    }

    if (ball_tile_at_offset(ball.x, ball.y, BALL_HOLE_PROBE_PIXELS, 0) == TILE_ID_HOLE) {
        return 1;
    }

    if (ball_tile_at_offset(ball.x, ball.y, -BALL_HOLE_PROBE_PIXELS, 0) == TILE_ID_HOLE) {
        return 1;
    }

    if (ball_tile_at_offset(ball.x, ball.y, 0, BALL_HOLE_PROBE_PIXELS) == TILE_ID_HOLE) {
        return 1;
    }

    if (ball_tile_at_offset(ball.x, ball.y, 0, -BALL_HOLE_PROBE_PIXELS) == TILE_ID_HOLE) {
        return 1;
    }

    return 0;
}

static uint8_t ball_over_water(void)
{
    return terrain_tile_is_water(ball_tile_at(ball.x, ball.y));
}

static void deflect_velocity(uint8_t tile)
{
    int16_t old_vx = ball.vx;
    int16_t old_vy = ball.vy;

    if (terrain_tile_is_slash_deflector(tile)) {
        ball.vx = physics_deflector_bounced_velocity((int16_t)(0 - old_vy));
        ball.vy = physics_deflector_bounced_velocity((int16_t)(0 - old_vx));
    } else {
        ball.vx = physics_deflector_bounced_velocity(old_vy);
        ball.vy = physics_deflector_bounced_velocity(old_vx);
    }
}

static void damp_ball_velocity(void)
{
    int16_t next_vx;
    int16_t next_vy;

    if (ball_is_airborne()) {
        next_vx = physics_damped_air_velocity(ball.vx);
        next_vy = physics_damped_air_velocity(ball.vy);
    } else {
        next_vx = physics_damped_velocity(ball.vx);
        next_vy = physics_damped_velocity(ball.vy);
    }

    if (ball.vx != 0 && ball.vy != 0 &&
        (physics_abs_i16(next_vx) <= BALL_DIAGONAL_STOP_SPEED ||
         physics_abs_i16(next_vy) <= BALL_DIAGONAL_STOP_SPEED)) {
        ball.vx = 0;
        ball.vy = 0;
        return;
    }

    ball.vx = next_vx;
    ball.vy = next_vy;
}

static void move_ball_outside_failed_ramp(void)
{
    uint8_t tile_x = coords_fixed_to_center_tile(ball.x);
    uint8_t tile_y = coords_fixed_to_center_tile(ball.y);
    int16_t center;

    if (physics_abs_i16(ball.vy) >= physics_abs_i16(ball.vx)) {
        if (ball.vy < 0) {
            center = (int16_t)(((uint16_t)(tile_y + 1) << TILE_PIXEL_SHIFT) +
                BALL_COLLISION_RADIUS_PIXELS + 1);
            ball.y = coords_center_pixel_to_fixed(center);
        } else {
            center = (int16_t)(((uint16_t)tile_y << TILE_PIXEL_SHIFT) -
                BALL_COLLISION_RADIUS_PIXELS - 1);
            ball.y = coords_center_pixel_to_fixed(center);
        }
    } else {
        if (ball.vx < 0) {
            center = (int16_t)(((uint16_t)(tile_x + 1) << TILE_PIXEL_SHIFT) +
                BALL_COLLISION_RADIUS_PIXELS + 1);
            ball.x = coords_center_pixel_to_fixed(center);
        } else {
            center = (int16_t)(((uint16_t)tile_x << TILE_PIXEL_SHIFT) -
                BALL_COLLISION_RADIUS_PIXELS - 1);
            ball.x = coords_center_pixel_to_fixed(center);
        }
    }
}

static void update_ball_sprite_position(void)
{
    uint16_t sprite_y;
    uint8_t arc_offset;

    if (ball_sprite == 0) {
        return;
    }

    ball_sprite->x = coords_fixed_to_sprite(ball.x);
    sprite_y = coords_fixed_to_sprite(ball.y);
    arc_offset = ramp_arc_offset();
    if (sprite_y > arc_offset) {
        sprite_y = (uint16_t)(sprite_y - arc_offset);
    } else {
        sprite_y = 0;
    }
    ball_sprite->y = sprite_y;

    if (ball_shadow_sprite == 0) {
        return;
    }

    ball_shadow_sprite->x = coords_fixed_to_sprite(ball.x);
    ball_shadow_sprite->y = coords_fixed_to_sprite(ball.y);
    ball_shadow_sprite->tile = ball_is_airborne() ? TILE_ID_BALL_SHADOW : EMPTY_TILE;
}

static uint8_t start_ramp_jump(void)
{
    uint8_t speed;
    uint8_t ramp_tile;

    if (!ramp_can_start()) {
        return 0;
    }

    ramp_tile = ball_tile_at(ball.x, ball.y);
    if (!ramp_tile_is(ramp_tile)) {
        return 0;
    }

    if (!ramp_accepts_direction(ramp_tile, ball.vx, ball.vy)) {
        return 0;
    }

    speed = physics_speed_u8(ball.vx, ball.vy);
    if (speed < RAMP_MIN_SPEED) {
        move_ball_outside_failed_ramp();
        ball.vx = ramp_failed_rebound_velocity(ball.vx);
        ball.vy = ramp_failed_rebound_velocity(ball.vy);
        ramp_start_failed_rebound();
        return 1;
    }

    ramp_start_jump(speed, &ball.vx, &ball.vy);
    return 0;
}

static void move_ball_x(void)
{
    int16_t remaining = ball.vx;

    while (remaining != 0) {
        int16_t step = physics_limited_motion_step(remaining);
        uint16_t next = physics_add_velocity_clamped(ball.x, step, BALL_MAX_X_FIXED);
        uint8_t tile = ball_collision_tile_x(next, ball.y, step);

        if (tile == TILE_ID_WALL) {
            ball.vx = physics_wall_bounced_velocity(ball.vx);
            return;
        }

        if (terrain_tile_is_deflector(tile)) {
            deflect_velocity(tile);
            return;
        }

        ball.x = next;
        remaining = (int16_t)(remaining - step);
    }
}

static void move_ball_y(void)
{
    int16_t remaining = ball.vy;

    while (remaining != 0) {
        int16_t step = physics_limited_motion_step(remaining);
        uint16_t next = physics_add_velocity_clamped(ball.y, step, BALL_MAX_Y_FIXED);
        uint8_t tile = ball_collision_tile_y(ball.x, next, step);

        if (tile == TILE_ID_WALL) {
            ball.vy = physics_wall_bounced_velocity(ball.vy);
            return;
        }

        if (terrain_tile_is_deflector(tile)) {
            deflect_velocity(tile);
            return;
        }

        ball.y = next;
        remaining = (int16_t)(remaining - step);
    }
}

static void ball_set_position(uint16_t x, uint16_t y)
{
    ball.x = (uint16_t)(x << FIXED_SHIFT);
    ball.y = (uint16_t)(y << FIXED_SHIFT);
    ball.vx = 0;
    ball.vy = 0;
    last_safe_x = ball.x;
    last_safe_y = ball.y;
    ramp_reset();
    water_hit = 0;
    hole_complete = 0;

    if (ball_sprite != 0) {
        ball_sprite->x = x;
        ball_sprite->y = y;
        ball_sprite->tile = TILE_ID_BALL;
    }
}

static void hide_aim_markers(void)
{
    uint8_t i;

    for (i = 0; i < AIM_MARKER_COUNT; i++) {
        aim_markers[i]->tile = EMPTY_TILE;
    }
}

static void show_aim_markers(void)
{
    uint8_t i;

    for (i = 0; i < AIM_MARKER_COUNT; i++) {
        aim_markers[i]->tile = TILE_ID_MARKER;
    }
}

static void ball_reset_to_last_safe(void)
{
    ball.x = last_safe_x;
    ball.y = last_safe_y;
    ball.vx = 0;
    ball.vy = 0;
    ramp_reset();
    hole_complete = 0;

    if (ball_sprite != 0) {
        update_ball_sprite_position();
        ball_sprite->tile = TILE_ID_BALL;
    }

    show_aim_markers();
    objects_update_aim_markers();
}

zos_err_t objects_init(void)
{
    gfx_error err;
    uint8_t i;
    const course_info_t* course = course_get_info();
    uint16_t ball_x = coords_sprite_from_tile(course->ball_x);
    uint16_t ball_y = coords_sprite_from_tile(course->ball_y);
    gfx_sprite sprite = {
        .y = ball_y,
        .x = ball_x,
        .tile = TILE_ID_BALL,
        .flags = SPRITE_NONE,
        .options = SPRITE_OPTION_NONE,
    };

    err = sprites_register_arena(sprite_arena, (uint8_t)SPRITE_ARENA_SIZE);
    if (err != GFX_SUCCESS) {
        return ERR_FAILURE;
    }

    ball_sprite = sprites_register(sprite);
    if (ball_sprite == 0) {
        return ERR_FAILURE;
    }

    ball_set_position(ball_x, ball_y);

    sprite.tile = EMPTY_TILE;
    ball_shadow_sprite = sprites_register(sprite);
    if (ball_shadow_sprite == 0) {
        return ERR_FAILURE;
    }

    sprite.tile = TILE_ID_MARKER;

    for (i = 0; i < AIM_MARKER_COUNT; i++) {
        sprite.x = ball_x;
        sprite.y = ball_y;
        aim_markers[i] = sprites_register(sprite);
        if (aim_markers[i] == 0) {
            return ERR_FAILURE;
        }
    }

    objects_update_aim_markers();

    return ERR_SUCCESS;
}

void objects_reset_ball_to_tile(uint8_t tile_x, uint8_t tile_y)
{
    ball_set_position(coords_sprite_from_tile(tile_x), coords_sprite_from_tile(tile_y));
    show_aim_markers();
    objects_update_aim_markers();
}

void objects_aim_left(void)
{
    if (objects_ball_is_rolling() || hole_complete) {
        return;
    }

    aim_rotate_left();
    objects_update_aim_markers();
}

void objects_aim_right(void)
{
    if (objects_ball_is_rolling() || hole_complete) {
        return;
    }

    aim_rotate_right();
    objects_update_aim_markers();
}

void objects_begin_charge(void)
{
    if (objects_ball_is_rolling() || hole_complete) {
        return;
    }

    water_hit = 0;
    shot_begin_charge();
}

void objects_charge_shot(void)
{
    if (objects_ball_is_rolling() || hole_complete) {
        return;
    }

    shot_charge_tick();
}

void objects_release_shot(void)
{
    int16_t dx;
    int16_t dy;

    if (objects_ball_is_rolling() || hole_complete) {
        return;
    }

    dx = aim_vector_x();
    dy = aim_vector_y();

    last_safe_x = ball.x;
    last_safe_y = ball.y;
    ball.vx = shot_velocity_from_vector(dx);
    ball.vy = shot_velocity_from_vector(dy);
    shot_count_stroke();
    hide_aim_markers();
}

uint8_t objects_get_power_segments(void)
{
    return shot_power_segments();
}

uint8_t objects_water_was_hit(void)
{
    return water_hit;
}

void objects_update_ball(void)
{
    if (!objects_ball_is_rolling()) {
        return;
    }

    move_ball_x();
    move_ball_y();

    if (!start_ramp_jump()) {
        damp_ball_velocity();
    }
    ramp_update();

    if (!ball_is_airborne() && ball_over_water()) {
        water_hit = 1;
        ball_reset_to_last_safe();
        return;
    }

    if (!ball_is_airborne()) {
        last_safe_x = ball.x;
        last_safe_y = ball.y;
    }

    if (!ball_is_airborne() &&
        ball_over_hole() &&
        physics_abs_i16(ball.vx) <= BALL_HOLE_MAX_SPEED &&
        physics_abs_i16(ball.vy) <= BALL_HOLE_MAX_SPEED) {
        ball.vx = 0;
        ball.vy = 0;
        hole_complete = 1;
        ball_sprite->tile = EMPTY_TILE;
        hide_aim_markers();
        return;
    }

    update_ball_sprite_position();

    if (!objects_ball_is_rolling() && !hole_complete) {
        show_aim_markers();
        objects_update_aim_markers();
    }
}

void objects_update_aim_markers(void)
{
    uint8_t i;
    int16_t dx = aim_vector_x();
    int16_t dy = aim_vector_y();

    if (ball_sprite == 0) {
        return;
    }

    if (objects_ball_is_rolling() || hole_complete) {
        return;
    }

    for (i = 0; i < AIM_MARKER_COUNT; i++) {
        uint16_t distance = (uint16_t)((i + 1) * AIM_MARKER_SPACING_PIXELS);
        aim_markers[i]->x = aim_marker_coord(ball_sprite->x, dx, distance);
        aim_markers[i]->y = aim_marker_coord(ball_sprite->y, dy, distance);
    }
}

uint8_t objects_ball_is_rolling(void)
{
    return (uint8_t)(ball.vx != 0 || ball.vy != 0 || ball_is_airborne());
}

uint8_t objects_hole_is_complete(void)
{
    return hole_complete;
}

uint16_t objects_get_strokes(void)
{
    return shot_get_strokes();
}

void objects_reset_strokes(void)
{
    shot_reset_strokes();
}

void objects_render(gfx_context* ctx)
{
    gfx_wait_vblank(ctx);
    sprites_render(ctx);
    gfx_wait_end_vblank(ctx);
}
