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

uint16_t aim_marker_coord(uint16_t origin, int16_t vector, uint16_t distance)
{
    return (uint16_t)((int16_t)origin + ((vector * (int16_t)distance) >> AIM_VECTOR_SHIFT));
}
