/**
 * @file    usbd_conf.h
 * @brief   Configuration of the ST USB device library (MSC on USB OTG FS).
 */
#ifndef USBD_CONF_H
#define USBD_CONF_H

#include "stm32f7xx_hal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define USBD_MAX_NUM_INTERFACES        1U
#define USBD_MAX_NUM_CONFIGURATION     1U
#define USBD_MAX_STR_DESC_SIZ          0x100U
#define USBD_SUPPORT_USER_STRING_DESC  0U
/* board powered from USB FS (JP1 = usb_fs): bus powered, 500 mA */
#define USBD_SELF_POWERED              0U
#define USBD_MAX_POWER                 0xFAU   /* in 2 mA units */
#define USBD_DEBUG_LEVEL               0U
#define USBD_LPM_ENABLED               0U

/* MSC: bytes transferred per SD access (multiple of 512) */
#define MSC_MEDIA_PACKET               4096U

#define DEVICE_FS                      0
#define DEVICE_HS                      1

/* static allocation instead of malloc */
#define USBD_malloc                    (void *)USBD_static_malloc
#define USBD_free                      USBD_static_free
#define USBD_memset                    memset
#define USBD_memcpy                    memcpy
#define USBD_Delay                     HAL_Delay

#define USBD_UsrLog(...)               do { } while (0)
#define USBD_ErrLog(...)               do { } while (0)
#define USBD_DbgLog(...)               do { } while (0)

void *USBD_static_malloc(uint32_t size);
void  USBD_static_free(void *p);

#endif
