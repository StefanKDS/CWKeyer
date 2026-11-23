/**
 * @file    usb_device.h
 * @brief   USB OTG FS (CN13) as mass storage device for the SD card.
 *
 * The drive is off by default: USB FS then only powers the board. When the
 * user switches it on, the device connects to the PC (D+ pull-up) and the
 * application stops using the SD card until it is switched off again.
 */
#ifndef USB_DEVICE_H
#define USB_DEVICE_H

#include <stdbool.h>

void usb_device_clock_config(void);   /**< 48 MHz from PLLSAI-P, call after display_init() */
bool usb_device_init(void);           /**< prepares the stack, stays disconnected */
void usb_device_enable(bool on);      /**< connect / disconnect the USB drive */
bool usb_device_active(void);         /**< drive switched on (SD card belongs to USB) */
bool usb_device_configured(void);     /**< PC has configured the drive */

#endif
