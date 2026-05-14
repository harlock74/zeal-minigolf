#include <stdint.h>

#include <zgdk/input.h>

#include "input.h"

enum {
    INPUT_USE_CONTROLLER = 1,
};

static uint8_t input_ready = 0;

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

    return (uint16_t)(keyboard_read() | input_read_controller());
}

void zgolf_input_flush(void)
{
    input_flush();
}
