/**
 * @file    touch.c
 */
#include "touch.h"
#include "stm32746g_discovery_ts.h"

static bool s_ok;

bool touch_init(void)
{
    s_ok = (BSP_TS_Init(480, 272) == TS_OK);
    return s_ok;
}

void touch_read(touch_t *t)
{
    TS_StateTypeDef st;
    t->down = false;
    if (!s_ok || BSP_TS_GetState(&st) != TS_OK)
        return;
    if (st.touchDetected)
    {
        t->down = true;
        t->x = (int16_t)st.touchX[0];
        t->y = (int16_t)st.touchY[0];
    }
}
