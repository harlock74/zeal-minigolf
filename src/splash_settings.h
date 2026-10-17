/* Fixed prompt and ball animation settings. Edit here; Tiled layers still build through the SDK. */
#pragma once
#include <stdint.h>
extern const uint8_t splash_course[];
extern const uint8_t splash_lettering[];
#define SPLASH_PROMPT_X 10
#define SPLASH_PROMPT_Y 16
#define SPLASH_PROMPT_WIDTH 19
#define SPLASH_BLINK_TICKS 36
#define SPLASH_PAUSE_TICKS 24
#define SPLASH_PATH_SEGMENTS 10
#define SPLASH_GROUND_Y 376
static const uint8_t splash_prompt_on[] = {31,33,20,34,34,255,34,35,16,33,35,255,35,30,255,31,27,16,40};
static const uint8_t splash_launch[] = {255,255,255,255,27,20,35,255,36,34,255,31,27,16,40,255,255,255,255};
static const uint16_t splash_path_x[] = {144,288,303,318,333,348,363,378,393,408,512};
static const uint16_t splash_path_y[] = {376,376,355,340,331,328,331,340,355,376,376};
static const uint16_t splash_path_ticks[] = {48,7,5,5,5,5,5,5,7,35};
static const int16_t splash_path_dx[] = {768,548,768,768,768,768,768,768,548,760};
static const int16_t splash_path_dy[] = {0,-768,-768,-460,-153,153,460,768,768,0};
