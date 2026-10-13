#include <stdint.h>
#include <zvb_sound.h>
#include "feedback.h"

static uint8_t sound_frames = 0;
static uint8_t suspended = 0;

static void mute_output(void)
{
    zvb_sound_set_volume(VOL_0);
    zvb_sound_set_channels(0, 0);
    zvb_sound_set_voices_vol(0x0f, VOL_0);
    zvb_sound_set_hold(0xff, 1);
}

void feedback_init(void)
{
    zvb_sound_initialize(1);
    sound_frames = 0;
    suspended = 0;
    mute_output();
}

void feedback_play(uint8_t effect)
{
    static const uint16_t dividers[] = { 780, 520, 1040, 220, 1560, 620, 1240 };
    static const uint8_t durations[] = { 3, 3, 8, 12, 18, 2, 6 };
    if (suspended || effect > FEEDBACK_FULL) {
        return;
    }
    sound_frames = durations[effect];
    /* HOLD freezes an oscillator; it is not a mute, especially for noise. */
    zvb_sound_set_channels(0, 0);
    zvb_sound_set_voices_vol(VOICE0, VOL_0);
    zvb_sound_set_voices(VOICE0, dividers[effect],
        effect == FEEDBACK_SPLASH ? WAV_NOISE : WAV_TRIANGLE);
    zvb_sound_set_voices_vol(VOICE0, VOL_25);
    zvb_sound_set_hold(VOICE0, 0);
    zvb_sound_set_channels(VOICE0, VOICE0);
    zvb_sound_set_volume(VOL_50);
}

void feedback_tick(void)
{
    if (suspended || (sound_frames > 0 && --sound_frames == 0)) mute_output();
}

void feedback_suspend(uint8_t quiet)
{
    suspended = quiet;
    sound_frames = 0;
    mute_output();
}

void feedback_stop(void)
{
    sound_frames = 0;
    zvb_sound_reset();
    /* Reset reconnects every oscillator at full voice volume: mute AFTER it. */
    mute_output();
}
