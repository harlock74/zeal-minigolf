#include <stdint.h>

#include <zgdk.h>
#include <zvb_gfx.h>
#include <zvb_sprite.h>

#include "config.h"
#include "course.h"
#include "objects.h"

static gfx_sprite sprite_arena[SPRITE_ARENA_SIZE];
static gfx_sprite* ball_sprite;
static gfx_sprite* ball_shadow_sprite;
static gfx_sprite* aim_markers[AIM_MARKER_COUNT];
static uint8_t aim_direction = 0;
static uint8_t shot_power_step = 0;
static uint8_t shot_charge_wait = 0;
static uint16_t strokes = 0;
static uint8_t hole_complete = 0;
static uint8_t water_hit = 0;
static uint8_t ramp_air_frames = 0;
static uint8_t ramp_air_total = 0;
static uint8_t ramp_cooldown = 0;
static uint16_t last_safe_x = 0;
static uint16_t last_safe_y = 0;

typedef struct {
    uint16_t x;
    uint16_t y;
    int16_t vx;
    int16_t vy;
} ball_state_t;

static ball_state_t ball;

typedef struct {
    int8_t x;
    int8_t y;
} aim_vector_t;

static const aim_vector_t aim_vectors[AIM_DIRECTION_COUNT] = {
    { 127,    0 },
    { 125,   25 },
    { 117,   49 },
    { 106,   71 },
    {  90,   90 },
    {  71,  106 },
    {  49,  117 },
    {  25,  125 },
    {   0,  127 },
    { -25,  125 },
    { -49,  117 },
    { -71,  106 },
    { -90,   90 },
    {-106,   71 },
    {-117,   49 },
    {-125,   25 },
    {-127,    0 },
    {-125,  -25 },
    {-117,  -49 },
    {-106,  -71 },
    { -90,  -90 },
    { -71, -106 },
    { -49, -117 },
    { -25, -125 },
    {   0, -127 },
    {  25, -125 },
    {  49, -117 },
    {  71, -106 },
    {  90,  -90 },
    { 106,  -71 },
    { 117,  -49 },
    { 125,  -25 },
};

static const uint8_t shot_power_steps[POWER_BAR_SEGMENTS] = SHOT_POWER_STEPS;

static uint16_t sprite_coord_from_tile(uint8_t tile)
{
    /*
     * ZVB sprite coordinates are biased by 16: coordinate 16 appears at
     * screen pixel 0. Keep the bias explicit so gameplay math can use pixels.
     */
    return (uint16_t)((tile * TILE_PIXEL_SIZE) + TILE_PIXEL_SIZE);
}

static uint16_t marker_coord(uint16_t origin, int16_t vector, uint16_t distance)
{
    return (uint16_t)((int16_t)origin + ((vector * (int16_t)distance) >> AIM_VECTOR_SHIFT));
}

static int16_t abs_i16(int16_t value)
{
    if (value < 0) {
        return (int16_t)-value;
    }

    return value;
}

static uint16_t fixed_to_sprite_coord(uint16_t value)
{
    return (uint16_t)(value >> FIXED_SHIFT);
}

static uint16_t fixed_to_center_pixel(uint16_t value)
{
    uint16_t coord = fixed_to_sprite_coord(value);

    if (coord > (TILE_PIXEL_SIZE / 2)) {
        coord = (uint16_t)(coord - (TILE_PIXEL_SIZE / 2));
    } else {
        coord = 0;
    }

    return coord;
}

static uint8_t fixed_to_center_tile(uint16_t value)
{
    return (uint8_t)(fixed_to_center_pixel(value) >> TILE_PIXEL_SHIFT);
}

static uint16_t center_pixel_to_fixed(int16_t value)
{
    if (value < 0) {
        value = 0;
    }

    return (uint16_t)(((uint16_t)value + (TILE_PIXEL_SIZE / 2)) << FIXED_SHIFT);
}

static uint8_t ball_tile_at(uint16_t x, uint16_t y)
{
    return course_get_tile(fixed_to_center_tile(x), fixed_to_center_tile(y));
}

static uint8_t ball_sample_outside_playfield(uint16_t x, uint16_t y)
{
    uint8_t tile_x = fixed_to_center_tile(x);
    uint8_t tile_y = fixed_to_center_tile(y);

    return (uint8_t)(tile_x >= COURSE_TILES_W || tile_y >= PLAYFIELD_TILES_H);
}

static uint8_t tile_is_water(uint8_t tile)
{
    return (uint8_t)(tile >= TILE_ID_WATER_FIRST && tile <= TILE_ID_WATER_LAST);
}

static uint8_t tile_is_slash_deflector(uint8_t tile)
{
    return (uint8_t)(tile == TILE_ID_DEFLECT_SLASH_1 || tile == TILE_ID_DEFLECT_SLASH_2);
}

static uint8_t tile_is_backslash_deflector(uint8_t tile)
{
    return (uint8_t)(tile == TILE_ID_DEFLECT_BACKSLASH_1 || tile == TILE_ID_DEFLECT_BACKSLASH_2);
}

static uint8_t tile_is_deflector(uint8_t tile)
{
    return (uint8_t)(tile_is_slash_deflector(tile) || tile_is_backslash_deflector(tile));
}

static uint8_t tile_is_ramp(uint8_t tile)
{
    return (uint8_t)(
        tile == TILE_ID_RAMP_LEFT ||
        tile == TILE_ID_RAMP_RIGHT ||
        tile == TILE_ID_RAMP_OPPOSITE_LEFT ||
        tile == TILE_ID_RAMP_OPPOSITE_RIGHT);
}

static uint8_t ball_is_airborne(void)
{
    return (uint8_t)(ramp_air_frames > 0);
}

static uint8_t deflector_solid_at(uint8_t tile, uint16_t x, uint16_t y)
{
    uint8_t local_x = (uint8_t)(fixed_to_center_pixel(x) & TILE_PIXEL_MASK);
    uint8_t local_y = (uint8_t)(fixed_to_center_pixel(y) & TILE_PIXEL_MASK);
    int16_t diagonal_y = local_x;
    int16_t distance;

    if (tile_is_slash_deflector(tile)) {
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
    if (tile_is_deflector(tile) &&
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
    if (tile == TILE_ID_WALL || tile_is_deflector(tile)) {
        return tile;
    }

    tile = ball_collision_tile_at_offset(x, y, edge, 0);
    if (tile == TILE_ID_WALL || tile_is_deflector(tile)) {
        return tile;
    }

    tile = ball_collision_tile_at_offset(x, y, edge, BALL_COLLISION_RADIUS_PIXELS);
    if (tile == TILE_ID_WALL || tile_is_deflector(tile)) {
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
    if (tile == TILE_ID_WALL || tile_is_deflector(tile)) {
        return tile;
    }

    tile = ball_collision_tile_at_offset(x, y, 0, edge);
    if (tile == TILE_ID_WALL || tile_is_deflector(tile)) {
        return tile;
    }

    tile = ball_collision_tile_at_offset(x, y, BALL_COLLISION_RADIUS_PIXELS, edge);
    if (tile == TILE_ID_WALL || tile_is_deflector(tile)) {
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
    return tile_is_water(ball_tile_at(ball.x, ball.y));
}

static int16_t bounced_velocity(int16_t velocity)
{
    uint16_t speed = (uint16_t)abs_i16(velocity);

    speed = (uint16_t)((speed * WALL_BOUNCE_FACTOR) >> FIXED_SHIFT);
    if (speed <= BALL_STOP_SPEED) {
        return 0;
    }

    if (velocity < 0) {
        return (int16_t)speed;
    }

    return (int16_t)(0 - speed);
}

static int16_t deflector_bounced_velocity(int16_t velocity)
{
    uint16_t speed = (uint16_t)abs_i16(velocity);

    speed = (uint16_t)((speed * DEFLECTOR_BOUNCE_FACTOR) >> FIXED_SHIFT);
    if (speed <= BALL_STOP_SPEED) {
        return 0;
    }

    if (velocity < 0) {
        return (int16_t)(0 - speed);
    }

    return (int16_t)speed;
}

static void deflect_velocity(uint8_t tile)
{
    int16_t old_vx = ball.vx;
    int16_t old_vy = ball.vy;

    if (tile_is_slash_deflector(tile)) {
        ball.vx = deflector_bounced_velocity((int16_t)(0 - old_vy));
        ball.vy = deflector_bounced_velocity((int16_t)(0 - old_vx));
    } else {
        ball.vx = deflector_bounced_velocity(old_vy);
        ball.vy = deflector_bounced_velocity(old_vx);
    }
}

static int16_t damp_velocity_with_friction(
    int16_t velocity,
    uint8_t friction,
    uint8_t friction_shift,
    uint8_t friction_rounding)
{
    uint16_t old_speed = (uint16_t)abs_i16(velocity);
    uint16_t speed = old_speed;

    if (old_speed == 0) {
        return 0;
    }

    speed = (uint16_t)(((speed * friction) + friction_rounding) >> friction_shift);
    if (speed >= old_speed) {
        speed = (uint16_t)(old_speed - 1);
    }

    if (speed <= BALL_STOP_SPEED) {
        return 0;
    }

    if (velocity < 0) {
        return (int16_t)(0 - speed);
    }

    return (int16_t)speed;
}

static int16_t damp_velocity(int16_t velocity)
{
    return damp_velocity_with_friction(
        velocity,
        BALL_FRICTION,
        BALL_FRICTION_SHIFT,
        BALL_FRICTION_ROUNDING);
}

static int16_t damp_air_velocity(int16_t velocity)
{
    return damp_velocity_with_friction(
        velocity,
        RAMP_AIR_FRICTION,
        RAMP_AIR_FRICTION_SHIFT,
        RAMP_AIR_FRICTION_ROUNDING);
}

static void damp_ball_velocity(void)
{
    int16_t next_vx;
    int16_t next_vy;

    if (ball_is_airborne()) {
        next_vx = damp_air_velocity(ball.vx);
        next_vy = damp_air_velocity(ball.vy);
    } else {
        next_vx = damp_velocity(ball.vx);
        next_vy = damp_velocity(ball.vy);
    }

    if (ball.vx != 0 && ball.vy != 0 &&
        (abs_i16(next_vx) <= BALL_DIAGONAL_STOP_SPEED ||
         abs_i16(next_vy) <= BALL_DIAGONAL_STOP_SPEED)) {
        ball.vx = 0;
        ball.vy = 0;
        return;
    }

    ball.vx = next_vx;
    ball.vy = next_vy;
}

static uint8_t ball_speed_for_ramp(void)
{
    uint16_t speed = (uint16_t)abs_i16(ball.vx);
    uint16_t speed_y = (uint16_t)abs_i16(ball.vy);

    if (speed_y > speed) {
        speed = speed_y;
    }

    if (speed > 255) {
        return 255;
    }

    return (uint8_t)speed;
}

static int16_t failed_ramp_rebound_velocity(int16_t velocity)
{
    uint16_t speed = (uint16_t)abs_i16(velocity);

    if (speed == 0) {
        return 0;
    }

    speed = (uint16_t)((speed * RAMP_FAILED_REBOUND_NUMERATOR) >> RAMP_FAILED_REBOUND_SHIFT);
    if (speed < RAMP_FAILED_MIN_REBOUND_SPEED) {
        speed = RAMP_FAILED_MIN_REBOUND_SPEED;
    }

    if (velocity < 0) {
        return (int16_t)speed;
    }

    return (int16_t)(0 - speed);
}

static void move_ball_outside_failed_ramp(void)
{
    uint8_t tile_x = fixed_to_center_tile(ball.x);
    uint8_t tile_y = fixed_to_center_tile(ball.y);
    int16_t center;

    if (abs_i16(ball.vy) >= abs_i16(ball.vx)) {
        if (ball.vy < 0) {
            center = (int16_t)(((uint16_t)(tile_y + 1) << TILE_PIXEL_SHIFT) +
                BALL_COLLISION_RADIUS_PIXELS + 1);
            ball.y = center_pixel_to_fixed(center);
        } else {
            center = (int16_t)(((uint16_t)tile_y << TILE_PIXEL_SHIFT) -
                BALL_COLLISION_RADIUS_PIXELS - 1);
            ball.y = center_pixel_to_fixed(center);
        }
    } else {
        if (ball.vx < 0) {
            center = (int16_t)(((uint16_t)(tile_x + 1) << TILE_PIXEL_SHIFT) +
                BALL_COLLISION_RADIUS_PIXELS + 1);
            ball.x = center_pixel_to_fixed(center);
        } else {
            center = (int16_t)(((uint16_t)tile_x << TILE_PIXEL_SHIFT) -
                BALL_COLLISION_RADIUS_PIXELS - 1);
            ball.x = center_pixel_to_fixed(center);
        }
    }
}

static uint8_t ramp_arc_offset(void)
{
    uint8_t half;
    uint8_t progress;

    if (!ball_is_airborne() || ramp_air_total < 2) {
        return 0;
    }

    half = (uint8_t)(ramp_air_total >> 1);
    progress = (uint8_t)(ramp_air_total - ramp_air_frames);
    if (progress > half) {
        progress = (uint8_t)(ramp_air_total - progress);
    }

    return (uint8_t)((progress * RAMP_ARC_MAX_PIXELS) / half);
}

static void update_ball_sprite_position(void)
{
    uint16_t sprite_y;
    uint8_t arc_offset;

    if (ball_sprite == 0) {
        return;
    }

    ball_sprite->x = fixed_to_sprite_coord(ball.x);
    sprite_y = fixed_to_sprite_coord(ball.y);
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

    ball_shadow_sprite->x = fixed_to_sprite_coord(ball.x);
    ball_shadow_sprite->y = fixed_to_sprite_coord(ball.y);
    ball_shadow_sprite->tile = ball_is_airborne() ? TILE_ID_BALL_SHADOW : EMPTY_TILE;
}

static uint8_t start_ramp_jump(void)
{
    uint8_t air_frames;

    if (ball_is_airborne() || ramp_cooldown > 0) {
        return 0;
    }

    if (!tile_is_ramp(ball_tile_at(ball.x, ball.y))) {
        return 0;
    }

    if (ball_speed_for_ramp() < RAMP_MIN_SPEED) {
        move_ball_outside_failed_ramp();
        ball.vx = failed_ramp_rebound_velocity(ball.vx);
        ball.vy = failed_ramp_rebound_velocity(ball.vy);
        ramp_cooldown = RAMP_FAILED_COOLDOWN_FRAMES;
        return 1;
    }

    air_frames = (uint8_t)(ball_speed_for_ramp() >> RAMP_AIR_SPEED_SHIFT);
    if (air_frames < RAMP_MIN_AIR_FRAMES) {
        air_frames = RAMP_MIN_AIR_FRAMES;
    }
    if (air_frames > RAMP_MAX_AIR_FRAMES) {
        air_frames = RAMP_MAX_AIR_FRAMES;
    }

    ramp_air_frames = air_frames;
    ramp_air_total = air_frames;
    ramp_cooldown = RAMP_COOLDOWN_FRAMES;
    ball.vx = (int16_t)((ball.vx * RAMP_LAUNCH_SPEED_NUMERATOR) >> RAMP_LAUNCH_SPEED_SHIFT);
    ball.vy = (int16_t)((ball.vy * RAMP_LAUNCH_SPEED_NUMERATOR) >> RAMP_LAUNCH_SPEED_SHIFT);
    return 0;
}

static void update_ramp_state(void)
{
    if (ramp_cooldown > 0) {
        ramp_cooldown--;
    }

    if (ramp_air_frames > 0) {
        ramp_air_frames--;
        if (ramp_air_frames == 0) {
            ramp_air_total = 0;
        }
    }
}

static uint8_t power_bar_fill_count(void)
{
    return (uint8_t)(shot_power_step + 1);
}

static int16_t limited_motion_step(int16_t remaining)
{
    if (remaining > BALL_MAX_STEP_FIXED) {
        return BALL_MAX_STEP_FIXED;
    }

    if (remaining < -BALL_MAX_STEP_FIXED) {
        return -BALL_MAX_STEP_FIXED;
    }

    return remaining;
}

static uint16_t add_velocity_clamped(uint16_t position, int16_t velocity, uint16_t max_position)
{
    int16_t next = (int16_t)position + velocity;

    if (next < BALL_MIN_FIXED) {
        return BALL_MIN_FIXED;
    }

    if ((uint16_t)next > max_position) {
        return max_position;
    }

    return (uint16_t)next;
}

static void move_ball_x(void)
{
    int16_t remaining = ball.vx;

    while (remaining != 0) {
        int16_t step = limited_motion_step(remaining);
        uint16_t next = add_velocity_clamped(ball.x, step, BALL_MAX_X_FIXED);
        uint8_t tile = ball_collision_tile_x(next, ball.y, step);

        if (tile == TILE_ID_WALL) {
            ball.vx = bounced_velocity(ball.vx);
            return;
        }

        if (tile_is_deflector(tile)) {
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
        int16_t step = limited_motion_step(remaining);
        uint16_t next = add_velocity_clamped(ball.y, step, BALL_MAX_Y_FIXED);
        uint8_t tile = ball_collision_tile_y(ball.x, next, step);

        if (tile == TILE_ID_WALL) {
            ball.vy = bounced_velocity(ball.vy);
            return;
        }

        if (tile_is_deflector(tile)) {
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
    ramp_air_frames = 0;
    ramp_air_total = 0;
    ramp_cooldown = 0;
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
    ramp_air_frames = 0;
    ramp_air_total = 0;
    ramp_cooldown = 0;
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
    uint16_t ball_x = sprite_coord_from_tile(course->ball_x);
    uint16_t ball_y = sprite_coord_from_tile(course->ball_y);
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
    ball_set_position(sprite_coord_from_tile(tile_x), sprite_coord_from_tile(tile_y));
    show_aim_markers();
    objects_update_aim_markers();
}

void objects_aim_left(void)
{
    if (objects_ball_is_rolling() || hole_complete) {
        return;
    }

    if (aim_direction == 0) {
        aim_direction = AIM_DIRECTION_COUNT - 1;
    } else {
        aim_direction--;
    }

    objects_update_aim_markers();
}

void objects_aim_right(void)
{
    if (objects_ball_is_rolling() || hole_complete) {
        return;
    }

    aim_direction++;
    if (aim_direction >= AIM_DIRECTION_COUNT) {
        aim_direction = 0;
    }

    objects_update_aim_markers();
}

void objects_begin_charge(void)
{
    if (objects_ball_is_rolling() || hole_complete) {
        return;
    }

    water_hit = 0;
    shot_power_step = 0;
    shot_charge_wait = POWER_BAR_STEP_FRAMES;
}

void objects_charge_shot(void)
{
    if (objects_ball_is_rolling() || hole_complete) {
        return;
    }

    if (shot_charge_wait > 0) {
        shot_charge_wait--;
        return;
    }

    shot_charge_wait = POWER_BAR_STEP_FRAMES;

    if (shot_power_step < (POWER_BAR_SEGMENTS - 1)) {
        shot_power_step++;
    } else {
        shot_power_step = 0;
    }
}

void objects_release_shot(void)
{
    int16_t dx;
    int16_t dy;

    if (objects_ball_is_rolling() || hole_complete) {
        return;
    }

    dx = aim_vectors[aim_direction].x;
    dy = aim_vectors[aim_direction].y;

    last_safe_x = ball.x;
    last_safe_y = ball.y;
    ball.vx = (int16_t)((dx * shot_power_steps[shot_power_step]) >> SHOT_VELOCITY_SHIFT);
    ball.vy = (int16_t)((dy * shot_power_steps[shot_power_step]) >> SHOT_VELOCITY_SHIFT);
    strokes++;
    hide_aim_markers();
}

uint8_t objects_get_power_segments(void)
{
    return power_bar_fill_count();
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
    update_ramp_state();

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
        abs_i16(ball.vx) <= BALL_HOLE_MAX_SPEED &&
        abs_i16(ball.vy) <= BALL_HOLE_MAX_SPEED) {
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
    int16_t dx = aim_vectors[aim_direction].x;
    int16_t dy = aim_vectors[aim_direction].y;

    if (ball_sprite == 0) {
        return;
    }

    if (objects_ball_is_rolling() || hole_complete) {
        return;
    }

    for (i = 0; i < AIM_MARKER_COUNT; i++) {
        uint16_t distance = (uint16_t)((i + 1) * AIM_MARKER_SPACING_PIXELS);
        aim_markers[i]->x = marker_coord(ball_sprite->x, dx, distance);
        aim_markers[i]->y = marker_coord(ball_sprite->y, dy, distance);
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
    return strokes;
}

void objects_reset_strokes(void)
{
    strokes = 0;
}

void objects_render(gfx_context* ctx)
{
    gfx_wait_vblank(ctx);
    sprites_render(ctx);
    gfx_wait_end_vblank(ctx);
}
