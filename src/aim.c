#include <stdint.h>

#include <zgdk/input.h>

#include "aim.h"
#include "config.h"

typedef struct {
    int8_t x;
    int8_t y;
} aim_vector_t;

static uint8_t aim_direction = 0;

static const aim_vector_t aim_vectors[AIM_DIRECTION_COUNT] = {
    {  127,    0 },
    {  126,   12 },
    {  125,   25 },
    {  122,   37 },
    {  117,   49 },
    {  112,   60 },
    {  106,   71 },
    {   98,   81 },
    {   90,   90 },
    {   81,   98 },
    {   71,  106 },
    {   60,  112 },
    {   49,  117 },
    {   37,  122 },
    {   25,  125 },
    {   12,  126 },
    {    0,  127 },
    {  -12,  126 },
    {  -25,  125 },
    {  -37,  122 },
    {  -49,  117 },
    {  -60,  112 },
    {  -71,  106 },
    {  -81,   98 },
    {  -90,   90 },
    {  -98,   81 },
    { -106,   71 },
    { -112,   60 },
    { -117,   49 },
    { -122,   37 },
    { -125,   25 },
    { -126,   12 },
    { -127,    0 },
    { -126,  -12 },
    { -125,  -25 },
    { -122,  -37 },
    { -117,  -49 },
    { -112,  -60 },
    { -106,  -71 },
    {  -98,  -81 },
    {  -90,  -90 },
    {  -81,  -98 },
    {  -71, -106 },
    {  -60, -112 },
    {  -49, -117 },
    {  -37, -122 },
    {  -25, -125 },
    {  -12, -126 },
    {    0, -127 },
    {   12, -126 },
    {   25, -125 },
    {   37, -122 },
    {   49, -117 },
    {   60, -112 },
    {   71, -106 },
    {   81,  -98 },
    {   90,  -90 },
    {   98,  -81 },
    {  106,  -71 },
    {  112,  -60 },
    {  117,  -49 },
    {  122,  -37 },
    {  125,  -25 },
    {  126,  -12 },
};

void aim_rotate_left(void)
{
    if (aim_direction == 0) {
        aim_direction = AIM_DIRECTION_COUNT - 1;
    } else {
        aim_direction--;
    }
}

void aim_rotate_right(void)
{
    aim_direction++;
    if (aim_direction >= AIM_DIRECTION_COUNT) {
        aim_direction = 0;
    }
}

int16_t aim_vector_x(void)
{
    return aim_vectors[aim_direction].x;
}

int16_t aim_vector_y(void)
{
    return aim_vectors[aim_direction].y;
}

void aim_reset_towards(int16_t dx, int16_t dy)
{
    uint8_t i;
    int32_t best = -2147483647L;
    for (i = 0; i < AIM_DIRECTION_COUNT; i++) {
        int32_t dot = (int32_t)dx * aim_vectors[i].x + (int32_t)dy * aim_vectors[i].y;
        if (dot > best) {
            best = dot;
            aim_direction = i;
        }
    }
}

uint16_t aim_marker_coord(uint16_t origin, int16_t vector, uint16_t distance)
{
    int16_t coord = (int16_t)origin + (vector * (int16_t)distance) / (1 << AIM_VECTOR_SHIFT);
    if (coord < TILE_PIXEL_SIZE) {
        coord = TILE_PIXEL_SIZE;
    }
    return (uint16_t)coord;
}
