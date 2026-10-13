#include <stdint.h>

#include <core.h>
#include <zos_keyboard.h>
#include <zos_sys.h>
#include <zos_vfs.h>
#include <zgdk/input.h>

#include "input.h"
#include "config.h"

enum {
    INPUT_USE_CONTROLLER = 1,
    KEYBOARD_BUFFER_SIZE = 64,
    KEYBOARD_MAX_BATCHES = 4,
};

static uint8_t input_ready = 0;
/* ZealOS read buffers must not straddle an MMU page. */
static uint8_t keyboard_storage[KEYBOARD_BUFFER_SIZE * 2 - 1];
static uint8_t* keyboard_buffer;
static uint8_t keyboard_release_pending = 0;
static uint16_t keyboard_state = 0;
static uint16_t physical_keys = 0;
static uint16_t controller_state = 0;
static uint16_t pressed_events = 0;
static uint16_t released_events = 0;
static uint8_t left_lease = 0;
static uint8_t right_lease = 0;
static uint8_t keyboard_aim_owner = 0;

static uint16_t current_state(void)
{
    uint16_t pad = controller_state;
    if (keyboard_aim_owner) pad &= (uint16_t)~(BUTTON_LEFT | BUTTON_RIGHT);
    return keyboard_state | pad;
}

static void note_transition(uint16_t before, uint16_t after)
{
    pressed_events |= (uint16_t)(after & ~before);
    released_events |= (uint16_t)(before & ~after);
}

static void keyboard_derive_state(void)
{
    uint16_t before = current_state();
    /* Bits 0..11 use the controller mapping; 12/13 are keyboard aliases. */
    keyboard_state = physical_keys & 0x0fff;
    if (physical_keys & 0x1000) keyboard_state |= BUTTON_B;
    if (physical_keys & 0x2000) keyboard_state |= BUTTON_SELECT;
    note_transition(before, current_state());
}

static void keyboard_apply_key(uint8_t key, uint8_t released)
{
    uint16_t bit;
    switch (key) {
    case KB_UP_ARROW: bit=BUTTON_UP; break;
    case KB_DOWN_ARROW: bit=BUTTON_DOWN; break;
    case KB_LEFT_ARROW: bit=BUTTON_LEFT; break;
    case KB_RIGHT_ARROW: bit=BUTTON_RIGHT; break;
    case KB_KEY_ENTER: bit=BUTTON_START; break;
    case KB_RIGHT_SHIFT: bit=BUTTON_SELECT; break;
    case KB_KEY_QUOTE: bit=0x2000; break;
    case KB_KEY_Z: bit=BUTTON_B; break;
    case KB_KEY_SPACE: bit=0x1000; break;
    case KB_KEY_X: bit=BUTTON_A; break;
    case KB_KEY_A: bit=BUTTON_Y; break;
    case KB_KEY_S: bit=BUTTON_X; break;
    case KB_KEY_Q: bit=BUTTON_L; break;
    case KB_KEY_W: bit=BUTTON_R; break;
    default: return;
    }
    if (bit == BUTTON_LEFT || bit == BUTTON_RIGHT) {
        uint16_t before = current_state();
        uint8_t lease = released ? 0 :
            (physical_keys & bit) ? 8 : KEYBOARD_AIM_STALE_FRAMES;
        /* Keep ownership after release: a phantom pad direction must never
         * resurrect a released keyboard arrow. Pad Start switches back. */
        keyboard_aim_owner = 1;
        note_transition(before, current_state());
        if (bit == BUTTON_LEFT) left_lease = lease;
        else right_lease = lease;
    }
    if(released) physical_keys &= (uint16_t)~bit;
    else physical_keys |= bit;
    keyboard_derive_state();
}

static uint16_t input_read_keyboard(void)
{
    uint8_t batch;
    /* Bound work per frame. Preserve a release prefix across reads. */
    for (batch=0; batch<KEYBOARD_MAX_BATCHES; batch++) {
        uint16_t size = KEYBOARD_BUFFER_SIZE;
        uint8_t i;
        zos_err_t err = read(DEV_STDIN, keyboard_buffer, &size);
        if (err != ERR_SUCCESS) {
            keyboard_state = 0;
            physical_keys = 0;
            keyboard_release_pending = 0;
            return keyboard_state;
        }

        if (size == 0) {
            return keyboard_state;
        }

        for (i = 0; i < size; i++) {
            if (keyboard_buffer[i] == KB_RELEASED) {
                keyboard_release_pending = 1;
            } else {
                keyboard_apply_key(keyboard_buffer[i], keyboard_release_pending);
                keyboard_release_pending = 0;
            }
        }

    }
    return keyboard_state;
}

static uint16_t input_read_controller(void)
{
    uint16_t controller_input = controller_read();
    uint16_t unused_bits = (uint16_t)(
        BUTTON_UNUSED1 |
        BUTTON_UNUSED2 |
        BUTTON_UNUSED3 |
        BUTTON_UNUSED4);
    uint16_t controller_buttons = (uint16_t)(
        BUTTON_B |
        BUTTON_Y |
        BUTTON_SELECT |
        BUTTON_START |
        BUTTON_UP |
        BUTTON_DOWN |
        BUTTON_LEFT |
        BUTTON_RIGHT |
        BUTTON_A |
        BUTTON_X |
        BUTTON_L |
        BUTTON_R);

    /*
     * Detached/noisy controller lines can look like impossible input states.
     * Ignore them so keyboard input is not hidden by floating SNES bits.
     */
    if (controller_input & unused_bits) {
        return 0;
    }

    if ((controller_input & controller_buttons) == controller_buttons) {
        return 0;
    }

    return (uint16_t)(controller_input & controller_buttons);
}

zos_err_t zgolf_input_init(void)
{
    keyboard_buffer = (uint8_t*)((uintptr_t)(keyboard_storage + KEYBOARD_BUFFER_SIZE - 1) &
        ~(uintptr_t)(KEYBOARD_BUFFER_SIZE - 1));
    zos_err_t err = input_init(INPUT_USE_CONTROLLER);

    if (err != ERR_SUCCESS) {
        input_ready = 0;
        return err;
    }

    input_ready = 1;
    zgolf_input_flush();
    return ERR_SUCCESS;
}

uint16_t zgolf_input_read_current(void)
{
    if (!input_ready) {
        return 0;
    }

    pressed_events = 0;
    released_events = 0;
    {
        uint16_t previous = current_state();
        uint16_t old_pad = controller_state;
        controller_state = input_read_controller();
        if ((controller_state & BUTTON_START) && !(old_pad & BUTTON_START)) {
            keyboard_aim_owner = 0;
            physical_keys &= (uint16_t)~(BUTTON_LEFT | BUTTON_RIGHT);
            keyboard_state &= (uint16_t)~(BUTTON_LEFT | BUTTON_RIGHT);
            left_lease = right_lease = 0;
        }
        note_transition(previous, current_state());
    }
    if (left_lease && --left_lease==0) physical_keys &= (uint16_t)~BUTTON_LEFT;
    if (right_lease && --right_lease==0) physical_keys &= (uint16_t)~BUTTON_RIGHT;
    keyboard_derive_state();
    input_read_keyboard();
    return current_state();
}

uint16_t zgolf_input_pressed(void) { return pressed_events; }
uint16_t zgolf_input_released(void) { return released_events; }

void zgolf_input_flush(void)
{
    /* Avoid the SDK flush's tiny read buffer splitting a release pair. */
    input_read_keyboard();
    keyboard_state = 0;
    physical_keys = 0;
    controller_state = 0;
    pressed_events = 0;
    released_events = 0;
    left_lease = right_lease = 0;
    keyboard_release_pending = 0;
}
