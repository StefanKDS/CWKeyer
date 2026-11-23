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

void touch_read(touch_t *t, bool rotate180)
{
    TS_StateTypeDef st;
    t->down = false;
    if (!s_ok || BSP_TS_GetState(&st) != TS_OK)
        return;
    if (st.touchDetected)
    {
        t->down = true;
        int x = st.touchX[0], y = st.touchY[0];
        if (rotate180)
        {
            x = 479 - x;
            y = 271 - y;
        }
        t->x = (int16_t)x;
        t->y = (int16_t)y;
    }
}
