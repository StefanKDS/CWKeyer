/**
 * @file    touch.h
 * @brief   FT5336 capacitive touch via BSP.
 */
#ifndef TOUCH_H
#define TOUCH_H

#include <stdbool.h>
#include "ui.h"

bool touch_init(void);
void touch_read(touch_t *t);

#endif
