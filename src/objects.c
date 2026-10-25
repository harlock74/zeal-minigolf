#include <stdint.h>

#include <zgdk.h>
#include <zvb_gfx.h>
#include <zvb_sprite.h>

#include "aim.h"
#include "config.h"
#include "coords.h"
#include "course.h"
#include "feedback.h"
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
static uint16_t shot_lie_x = 0;
static uint16_t shot_lie_y = 0;
static int16_t motion_remainder_x = 0;
static int16_t motion_remainder_y = 0;
static uint8_t sink_frames = 0;
static uint8_t splash_frames = 0;

static void hide_aim_markers(void);

typedef struct {
    uint16_t x;
    uint16_t y;
    int16_t vx;
    int16_t vy;
} ball_state_t;

static ball_state_t ball;
static uint8_t sample_cache_valid = 0;
static uint8_t sample_cache_x;
static uint8_t sample_cache_y;
static uint8_t sample_cache_tile;
static uint8_t sample_tiles_x[4];
static uint8_t sample_tiles_y[4];
static uint8_t sample_tiles[4];

static void reset_sample_cache(void)
{
    sample_cache_valid = 0;
    sample_tiles_x[0] = sample_tiles_x[1] = sample_tiles_x[2] = sample_tiles_x[3] = 255;
}

/* The course is immutable during an update. Reuse only the raw tile ID;
 * diagonal-bank geometry is still tested separately at every edge sample. */
static uint8_t sample_tile_miss(uint8_t x, uint8_t y)
{
    /* Adjacent edge samples can alternate between floor and wall tiles.
     * Keep their raw IDs instead of repeatedly evicting one another.
     * Exact bank geometry is still evaluated at every sample below. */
    uint8_t slot = (uint8_t)((x & 1) | ((y & 1) << 1));
    if (sample_tiles_x[slot] != x || sample_tiles_y[slot] != y) {
        sample_tiles[slot] = course_get_tile(x, y);
        sample_tiles_x[slot] = x;
        sample_tiles_y[slot] = y;
    }
    sample_cache_tile = sample_tiles[slot];
    sample_cache_x = x;
    sample_cache_y = y;
    sample_cache_valid = 1;
    return sample_cache_tile;
}

static uint8_t sampled_course_tile(uint8_t x, uint8_t y)
{
    /* Keep the common repeated-sample hit free of the indexed cache's stack
     * frame. Only changes of tile need the four-entry lookup. */
    if (sample_cache_valid && x == sample_cache_x && y == sample_cache_y) {
        return sample_cache_tile;
    }
    return sample_tile_miss(x, y);
}

static uint8_t ball_tile_at(uint16_t x, uint16_t y)
{
    return sampled_course_tile(coords_fixed_to_center_tile(x), coords_fixed_to_center_tile(y));
}

static uint8_t ball_is_airborne(void)
{
    return ramp_is_airborne();
}

static uint8_t deflector_solid_at(uint8_t tile, uint16_t x, uint16_t y)
{
    /* The same saturated centre-pixel coordinates, using only the low pixel
     * nibble for bank geometry. No nested conversions or signed word maths
     * on the high-speed bank path; the solid band is exactly unchanged. */
    uint8_t local_x = x < (TILE_PIXEL_SIZE / 2) * FIXED_ONE ? 0 :
        (uint8_t)(((x >> FIXED_SHIFT) - TILE_PIXEL_SIZE / 2) & TILE_PIXEL_MASK);
    uint8_t local_y = y < (TILE_PIXEL_SIZE / 2) * FIXED_ONE ? 0 :
        (uint8_t)(((y >> FIXED_SHIFT) - TILE_PIXEL_SIZE / 2) & TILE_PIXEL_MASK);
    uint8_t diagonal_y = local_x;
    uint8_t distance;

    if (tile == TILE_ID_DEFLECT_SLASH_1 || tile == TILE_ID_DEFLECT_SLASH_2) {
        diagonal_y = (uint8_t)(TILE_PIXEL_MASK - local_x);
    }

    distance = local_y > diagonal_y ? local_y - diagonal_y : diagonal_y - local_y;

    return (uint8_t)(distance <= DEFLECTOR_SOLID_HALF_WIDTH_PIXELS);
}

static uint8_t ball_collision_tile_at_sample(int16_t sample_x, int16_t sample_y)
{
    uint8_t tile_x;
    uint8_t tile_y;
    uint8_t tile;

    if (sample_x < BALL_MIN_FIXED || sample_y < BALL_MIN_FIXED) {
        return TILE_ID_WALL;
    }

    /* Equivalent to fixed -> sprite -> centre pixel -> tile, including the
     * centre-coordinate saturation at zero. Subtract the 8px sprite bias in
     * Q4, then shift by eight: SDCC can select the high byte directly.
     * Keep every edge sample and the <=2px subdivision unchanged. */
    tile_x = sample_x < (TILE_PIXEL_SIZE / 2) * FIXED_ONE ? 0 :
        (uint8_t)((sample_x - (TILE_PIXEL_SIZE / 2) * FIXED_ONE) >> (FIXED_SHIFT + TILE_PIXEL_SHIFT));
    tile_y = sample_y < (TILE_PIXEL_SIZE / 2) * FIXED_ONE ? 0 :
        (uint8_t)((sample_y - (TILE_PIXEL_SIZE / 2) * FIXED_ONE) >> (FIXED_SHIFT + TILE_PIXEL_SHIFT));
    if (tile_x >= COURSE_TILES_W || tile_y >= PLAYFIELD_TILES_H) {
        return TILE_ID_WALL;
    }

    tile = sampled_course_tile(tile_x, tile_y);
    if (terrain_tile_is_deflector(tile) &&
        !deflector_solid_at(tile, (uint16_t)sample_x, (uint16_t)sample_y)) {
        return EMPTY_TILE;
    }

    return tile;
}

static uint8_t ball_collision_tile_at_offset(uint16_t x, uint16_t y, int8_t offset_x, int8_t offset_y)
{
    return ball_collision_tile_at_sample((int16_t)x + (int16_t)offset_x * FIXED_ONE,
        (int16_t)y + (int16_t)offset_y * FIXED_ONE);
}

static uint8_t ball_collision_tile_x(uint16_t x, uint16_t y, int16_t velocity)
{
    int16_t edge = BALL_COLLISION_RADIUS_PIXELS * FIXED_ONE;
    uint8_t tile;

    if (velocity < 0) {
        edge = -BALL_COLLISION_RADIUS_PIXELS * FIXED_ONE;
    }

    int16_t sample_x = (int16_t)x + edge;
    tile = ball_collision_tile_at_sample(sample_x, (int16_t)y - BALL_COLLISION_RADIUS_PIXELS * FIXED_ONE);
    if (tile == TILE_ID_WALL || terrain_tile_is_deflector(tile)) {
        return tile;
    }

    tile = ball_collision_tile_at_sample(sample_x, (int16_t)y);
    if (tile == TILE_ID_WALL || terrain_tile_is_deflector(tile)) {
        return tile;
    }

    tile = ball_collision_tile_at_sample(sample_x, (int16_t)y + BALL_COLLISION_RADIUS_PIXELS * FIXED_ONE);
    if (tile == TILE_ID_WALL || terrain_tile_is_deflector(tile)) {
        return tile;
    }

    return EMPTY_TILE;
}

static uint8_t ball_collision_tile_y(uint16_t x, uint16_t y, int16_t velocity)
{
    int16_t edge = BALL_COLLISION_RADIUS_PIXELS * FIXED_ONE;
    uint8_t tile;

    if (velocity < 0) {
        edge = -BALL_COLLISION_RADIUS_PIXELS * FIXED_ONE;
    }

    int16_t sample_y = (int16_t)y + edge;
    tile = ball_collision_tile_at_sample((int16_t)x - BALL_COLLISION_RADIUS_PIXELS * FIXED_ONE, sample_y);
    if (tile == TILE_ID_WALL || terrain_tile_is_deflector(tile)) {
        return tile;
    }

    tile = ball_collision_tile_at_sample((int16_t)x, sample_y);
    if (tile == TILE_ID_WALL || terrain_tile_is_deflector(tile)) {
        return tile;
    }

    tile = ball_collision_tile_at_sample((int16_t)x + BALL_COLLISION_RADIUS_PIXELS * FIXED_ONE, sample_y);
    if (tile == TILE_ID_WALL || terrain_tile_is_deflector(tile)) {
        return tile;
    }

    return EMPTY_TILE;
}

static uint8_t ball_over_hole(void)
{
    uint16_t vx = (uint16_t)physics_abs_i16(ball.vx) >> VELOCITY_TO_POSITION_SHIFT;
    uint16_t vy = (uint16_t)physics_abs_i16(ball.vy) >> VELOCITY_TO_POSITION_SHIFT;
    /* Most substeps of a hard shot are too fast to sink. Reject them before
     * calculating the cup's fixed-point distance, with the same speed gate. */
    if (vx > BALL_HOLE_MAX_SPEED || vy > BALL_HOLE_MAX_SPEED) {
        return 0;
    }
    const course_info_t* info = course_get_info();
    int16_t dx = (int16_t)ball.x - (int16_t)(coords_sprite_from_tile(info->hole_x) * FIXED_ONE);
    int16_t dy = (int16_t)ball.y - (int16_t)(coords_sprite_from_tile(info->hole_y) * FIXED_ONE);
    uint16_t ax = (uint16_t)physics_abs_i16(dx);
    uint16_t ay = (uint16_t)physics_abs_i16(dy);
    uint16_t radius = BALL_HOLE_RADIUS_PIXELS * FIXED_ONE;
    if (ax > radius || ay > radius) {
        return 0;
    }
    return (uint8_t)(ax * ax + ay * ay <= radius * radius &&
        vx * vx + vy * vy <= BALL_HOLE_MAX_SPEED * BALL_HOLE_MAX_SPEED);
}

static uint8_t ball_over_water(void)
{
    return terrain_tile_is_water(ball_tile_at(ball.x, ball.y));
}

static void deflect_velocity(uint8_t tile)
{
    int16_t old_vx = ball.vx;
    int16_t old_vy = ball.vy;
    feedback_play(FEEDBACK_BANK);

    if (terrain_tile_is_slash_deflector(tile)) {
        ball.vx = physics_deflector_bounced_velocity(old_vy);
        ball.vy = physics_deflector_bounced_velocity(old_vx);
    } else {
        ball.vx = physics_deflector_bounced_velocity((int16_t)(0 - old_vy));
        ball.vy = physics_deflector_bounced_velocity((int16_t)(0 - old_vx));
    }
    physics_reset_damping();
    motion_remainder_x = 0;
    motion_remainder_y = 0;
}

static void damp_ball_velocity(void)
{
    physics_damp_vector(&ball.vx, &ball.vy, ball_is_airborne());
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

/* Trigger at the lip of the pair, not at an arbitrary point inside it. */
static uint8_t start_ramp_jump(uint16_t previous_x, uint16_t previous_y)
{
    uint8_t tile_x;
    uint8_t tile_y;
    uint8_t tile;
    uint8_t crossed;
    uint8_t speed;
    if (!ramp_can_start()) {
        return 0;
    }
    tile = ball_tile_at(previous_x, previous_y);
    if (!ramp_tile_is(tile) || !ramp_accepts_direction(tile, ball.vx, ball.vy)) {
        return 0;
    }
    tile_x = coords_fixed_to_center_tile(previous_x);
    tile_y = coords_fixed_to_center_tile(previous_y);
    if (tile == TILE_ID_RAMP_UP_LEFT || tile == TILE_ID_RAMP_UP_RIGHT) {
        crossed = (uint8_t)(coords_fixed_to_center_tile(ball.y) < tile_y);
    } else {
        crossed = (uint8_t)(coords_fixed_to_center_tile(ball.x) > tile_x);
    }
    if (!crossed) {
        return 0;
    }
    speed = physics_speed_u8(ball.vx, ball.vy);
    if (speed < RAMP_MIN_SPEED) {
        ball.x = previous_x;
        ball.y = previous_y;
        ball.vx = ramp_failed_rebound_velocity(ball.vx);
        ball.vy = ramp_failed_rebound_velocity(ball.vy);
        ramp_start_failed_rebound();
    } else {
        ramp_start_jump(speed, &ball.vx, &ball.vy);
        feedback_play(FEEDBACK_LAUNCH);
    }
    physics_reset_damping();
    motion_remainder_x = 0;
    motion_remainder_y = 0;
    return 1;
}

static uint8_t check_ground_contact(void)
{
    if (ball_is_airborne()) {
        return 0;
    }
    if (ball_over_water()) {
        feedback_play(FEEDBACK_SPLASH);
        water_hit = 1;
        splash_frames = BALL_SPLASH_FRAMES;
        ball.vx = 0;
        ball.vy = 0;
        hide_aim_markers();
        return 1;
    }
    if (ball_over_hole()) {
        const course_info_t* info = course_get_info();
        ball.x = coords_sprite_from_tile(info->hole_x) * FIXED_ONE;
        ball.y = coords_sprite_from_tile(info->hole_y) * FIXED_ONE;
        ball.vx = 0;
        ball.vy = 0;
        sink_frames = BALL_SINK_FRAMES;
        feedback_play(FEEDBACK_CUP);
        hide_aim_markers();
        return 1;
    }
    return 0;
}

static int16_t frame_motion(int16_t velocity, int16_t* remainder)
{
    int16_t sum = (int16_t)(velocity + *remainder);
    int16_t delta = sum / (1 << VELOCITY_TO_POSITION_SHIFT);
    *remainder = (int16_t)(sum - delta * (1 << VELOCITY_TO_POSITION_SHIFT));
    return delta;
}

/* Advance both axes from the same trajectory in <=2px substeps. Discard
 * remaining travel on a bank contact: never reflect twice in one frame. */
static void move_ball(void)
{
    reset_sample_cache();
    int16_t remaining_x = frame_motion(ball.vx, &motion_remainder_x);
    int16_t remaining_y = frame_motion(ball.vy, &motion_remainder_y);
    uint16_t max = (uint16_t)physics_abs_i16(remaining_x);
    uint8_t steps;
    uint8_t split_shift=0;
    uint8_t split_count;
    uint8_t fraction_x,fraction_y,error_x=0,error_y=0;
    int16_t base_x,base_y;
    if (physics_abs_i16(remaining_y) > max) {
        max = (uint16_t)physics_abs_i16(remaining_y);
    }
    /* Power-of-two subdivision replaces two software divisions per step.
     * Distribute residual Q4 units with independent error accumulators. */
    while(max>BALL_MAX_STEP_FIXED) {
        max=(uint16_t)((max+1)>>1);
        split_shift++;
    }
    split_count=(uint8_t)(1u<<split_shift);
    steps=split_count;
    base_x=(int16_t)((uint16_t)physics_abs_i16(remaining_x)>>split_shift);
    base_y=(int16_t)((uint16_t)physics_abs_i16(remaining_y)>>split_shift);
    fraction_x=(uint8_t)((uint16_t)physics_abs_i16(remaining_x)&(split_count-1));
    fraction_y=(uint8_t)((uint16_t)physics_abs_i16(remaining_y)&(split_count-1));
    if(remaining_x<0) base_x=(int16_t)-base_x;
    if(remaining_y<0) base_y=(int16_t)-base_y;
    while (steps > 0) {
        int16_t sx=base_x,sy=base_y;
        error_x+=fraction_x;error_y+=fraction_y;
        if(error_x>=split_count) {sx+=remaining_x<0 ? -1 : 1;error_x-=split_count;}
        if(error_y>=split_count) {sy+=remaining_y<0 ? -1 : 1;error_y-=split_count;}
        uint16_t old_x = ball.x;
        uint16_t old_y = ball.y;
        uint16_t nx = physics_add_velocity_clamped(ball.x, sx, BALL_MAX_X_FIXED);
        uint16_t ny = physics_add_velocity_clamped(ball.y, sy, BALL_MAX_Y_FIXED);
        /* Resolve each face against the position accepted on the other axis.
         * A side sample on a vertical wall must not reverse tangential Y. */
        uint8_t tx = sx != 0 ? ball_collision_tile_x(nx, old_y, sx) : EMPTY_TILE;
        uint16_t resolved_x = tx == TILE_ID_WALL ? old_x : nx;
        uint8_t ty = sy != 0 ? ball_collision_tile_y(resolved_x, ny, sy) : EMPTY_TILE;
        if (terrain_tile_is_deflector(tx) || terrain_tile_is_deflector(ty)) {
            deflect_velocity(terrain_tile_is_deflector(tx) ? tx : ty);
            return;
        }
        if (tx == TILE_ID_WALL) {
            feedback_play(FEEDBACK_BANK);
            ball.vx = physics_wall_bounced_velocity(ball.vx);
            physics_reset_damping();
            motion_remainder_x = 0;
            remaining_x = 0;
            base_x=0;fraction_x=0;error_x=0;
        } else {
            ball.x = nx;
        }
        if (ty == TILE_ID_WALL) {
            feedback_play(FEEDBACK_BANK);
            ball.vy = physics_wall_bounced_velocity(ball.vy);
            physics_reset_damping();
            motion_remainder_y = 0;
            remaining_y = 0;
            base_y=0;fraction_y=0;error_y=0;
        } else {
            ball.y = ny;
        }
        if (start_ramp_jump(old_x, old_y) || check_ground_contact()) {
            return;
        }
        steps--;
    }
}

static void ball_set_position(uint16_t x, uint16_t y)
{
    reset_sample_cache();
    ball.x = (uint16_t)(x << FIXED_SHIFT);
    ball.y = (uint16_t)(y << FIXED_SHIFT);
    ball.vx = 0;
    ball.vy = 0;
    shot_lie_x = ball.x;
    shot_lie_y = ball.y;
    ramp_reset();
    physics_reset_damping();
    motion_remainder_x = 0;
    motion_remainder_y = 0;
    sink_frames = 0;
    splash_frames = 0;
    water_hit = 0;
    hole_complete = 0;

    if (ball_sprite != 0) {
        ball_sprite->x = x;
        ball_sprite->y = y;
        ball_sprite->tile = TILE_ID_BALL;
        update_ball_sprite_position();
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

static void ball_reset_to_shot_lie(void)
{
    ball.x = shot_lie_x;
    ball.y = shot_lie_y;
    ball.vx = 0;
    ball.vy = 0;
    ramp_reset();
    physics_reset_damping();
    motion_remainder_x = 0;
    motion_remainder_y = 0;
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

    aim_reset_towards((int16_t)course->hole_x - course->ball_x,
        (int16_t)course->hole_y - course->ball_y);
    objects_update_aim_markers();

    return ERR_SUCCESS;
}

void objects_reset_ball_to_tile(uint8_t tile_x, uint8_t tile_y)
{
    ball_set_position(coords_sprite_from_tile(tile_x), coords_sprite_from_tile(tile_y));
    show_aim_markers();
    {
        const course_info_t* info = course_get_info();
        aim_reset_towards((int16_t)info->hole_x - tile_x, (int16_t)info->hole_y - tile_y);
    }
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

    shot_lie_x = ball.x;
    shot_lie_y = ball.y;
    ball.vx = shot_velocity_from_vector(dx);
    ball.vy = shot_velocity_from_vector(dy);
    physics_reset_damping();
    motion_remainder_x = 0;
    motion_remainder_y = 0;
    feedback_play(FEEDBACK_SHOT);
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
    uint8_t was_airborne;
    ramp_tick_cooldown();
    if (splash_frames > 0) {
        splash_frames--;
        ball_sprite->tile = splash_frames > BALL_SPLASH_FRAMES / 2 ? TILE_ID_BALL_SHADOW : EMPTY_TILE;
        if (splash_frames == 0) {
            ball_reset_to_shot_lie();
        }
        return;
    }
    if (sink_frames > 0) {
        sink_frames--;
        ball_sprite->tile = sink_frames > BALL_SINK_FRAMES / 2 ? TILE_ID_BALL_SHADOW : EMPTY_TILE;
        if (sink_frames == 0) {
            hole_complete = 1;
        }
        return;
    }
    if (!objects_ball_is_rolling()) {
        return;
    }
    was_airborne = ball_is_airborne();
    move_ball();
    if (splash_frames == 0 && sink_frames == 0) {
        damp_ball_velocity();
    }
    /* A new launch gets its full first airborne frame on the next tick. */
    if (was_airborne || !ball_is_airborne()) {
        ramp_update();
    }
    if (splash_frames == 0 && sink_frames == 0) {
        check_ground_contact();
    }
    update_ball_sprite_position();
    if (!objects_ball_is_rolling() && !hole_complete) {
        physics_reset_damping();
        motion_remainder_x = 0;
        motion_remainder_y = 0;
        show_aim_markers();
        objects_update_aim_markers();
    }
}

void objects_update_aim_markers(void)
{
    reset_sample_cache();
    uint8_t i;
    uint8_t blocked = 0;
    int16_t dx = aim_vector_x();
    int16_t dy = aim_vector_y();
    if (ball_sprite == 0 || objects_ball_is_rolling() || hole_complete) {
        return;
    }
    for (i = 0; i < AIM_MARKER_COUNT; i++) {
        uint16_t distance = (uint16_t)((i + 1) * AIM_MARKER_SPACING_PIXELS);
        uint8_t tile;
        aim_markers[i]->x = aim_marker_coord(ball_sprite->x, dx, distance);
        aim_markers[i]->y = aim_marker_coord(ball_sprite->y, dy, distance);
        tile = ball_collision_tile_at_offset(aim_markers[i]->x * FIXED_ONE,
            aim_markers[i]->y * FIXED_ONE, 0, 0);
        if (aim_markers[i]->x >= SCREEN_WIDTH + TILE_PIXEL_SIZE ||
            aim_markers[i]->y >= PLAYFIELD_HEIGHT + TILE_PIXEL_SIZE ||
            tile == TILE_ID_WALL || terrain_tile_is_deflector(tile)) {
            blocked = 1;
        }
        aim_markers[i]->tile = blocked ? EMPTY_TILE : TILE_ID_MARKER;
    }
}

uint8_t objects_ball_is_rolling(void)
{
    return (uint8_t)(ball.vx != 0 || ball.vy != 0 || ball_is_airborne() || sink_frames > 0 || splash_frames > 0);
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
