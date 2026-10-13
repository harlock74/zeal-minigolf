#ifndef ZGOLF_FEEDBACK_H
#define ZGOLF_FEEDBACK_H

#include <stdint.h>
enum { FEEDBACK_SHOT, FEEDBACK_BANK, FEEDBACK_LAUNCH, FEEDBACK_SPLASH, FEEDBACK_CUP,
    FEEDBACK_POWER, FEEDBACK_FULL };
void feedback_init(void);
void feedback_play(uint8_t effect);
void feedback_tick(void);
void feedback_stop(void);
void feedback_suspend(uint8_t quiet);

#endif
