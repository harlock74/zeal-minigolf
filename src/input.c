#include <stdint.h>

#include <core.h>
#include <zos_keyboard.h>
#include <zos_sys.h>
#include <zos_vfs.h>
#include <zgdk/input.h>

#include "input.h"

enum {
    INPUT_USE_CONTROLLER = 1,
    KEYBOARD_BUFFER_SIZE = 32,
};

static uint8_t input_ready = 0;
static uint8_t keyboard_buffer[KEYBOARD_BUFFER_SIZE];
static uint8_t keyboard_release_pending = 0;
static uint16_t keyboard_state = 0;

static void keyboard_apply_button(uint16_t button, uint8_t released)
{
    if (released) {
        keyboard_state &= (uint16_t)~button;
    } else {
        keyboard_state |= button;
    }
}

static void keyboard_apply_vertical(uint16_t button, uint16_t opposite, uint8_t released)
{
    if (released) {
        keyboard_state &= (uint16_t)~button;
    } else {
        keyboard_state = (uint16_t)((keyboard_state & (uint16_t)~opposite) | button);
    }
}

static void keyboard_apply_horizontal(uint16_t button, uint16_t opposite, uint8_t released)
{
    if (released) {
        keyboard_state &= (uint16_t)~button;
    } else {
        keyboard_state = (uint16_t)((keyboard_state & (uint16_t)~opposite) | button);
    }
}

static void keyboard_apply_key(uint8_t key, uint8_t released)
{
    switch (key) {
    case KB_UP_ARROW:
        keyboard_apply_vertical(BUTTON_UP, BUTTON_DOWN, released);
        break;
    case KB_DOWN_ARROW:
        keyboard_apply_vertical(BUTTON_DOWN, BUTTON_UP, released);
        break;
    case KB_LEFT_ARROW:
        keyboard_apply_horizontal(BUTTON_LEFT, BUTTON_RIGHT, released);
        break;
    case KB_RIGHT_ARROW:
        keyboard_apply_horizontal(BUTTON_RIGHT, BUTTON_LEFT, released);
        break;
    case KB_KEY_ENTER:
        keyboard_apply_button(BUTTON_START, released);
        break;
    case KB_RIGHT_SHIFT:
    case KB_KEY_QUOTE:
        keyboard_apply_button(BUTTON_SELECT, released);
        break;
    case KB_KEY_Z:
    case KB_KEY_SPACE:
        keyboard_apply_button(BUTTON_B, released);
        break;
    case KB_KEY_X:
        keyboard_apply_button(BUTTON_A, released);
        break;
    case KB_KEY_A:
        keyboard_apply_button(BUTTON_Y, released);
        break;
    case KB_KEY_S:
        keyboard_apply_button(BUTTON_X, released);
        break;
    case KB_KEY_Q:
        keyboard_apply_button(BUTTON_L, released);
        break;
    case KB_KEY_W:
        keyboard_apply_button(BUTTON_R, released);
        break;
    default:
        break;
    }
}

static uint16_t input_read_keyboard(void)
{
    uint16_t size = KEYBOARD_BUFFER_SIZE;

    while (1) {
        uint8_t i;
        zos_err_t err = read(DEV_STDIN, keyboard_buffer, &size);
        if (err != ERR_SUCCESS) {
            keyboard_state = 0;
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

        size = KEYBOARD_BUFFER_SIZE;
    }
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

    return (uint16_t)(input_read_keyboard() | input_read_controller());
}

void zgolf_input_flush(void)
{
    input_flush();
    keyboard_state = 0;
    keyboard_release_pending = 0;
}
