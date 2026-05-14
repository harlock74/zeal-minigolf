#ifndef ZGOLF_INPUT_H
#define ZGOLF_INPUT_H

#include <stdint.h>

#include <zos_errors.h>

zos_err_t zgolf_input_init(void);
uint16_t zgolf_input_read_current(void);
void zgolf_input_flush(void);

#endif
