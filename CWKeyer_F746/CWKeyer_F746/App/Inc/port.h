/**
 * @file    port.h
 * @brief   Platform abstraction used by the application layer.
 *
 * The application (App/ and Gui/) only talks to hardware through these
 * functions. Platform/ implements them for the STM32F746G-DISCO,
 * Tools/sim implements them for the PC simulator.
 */
#ifndef PORT_H
#define PORT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ---- time / system ---------------------------------------------------- */
uint32_t port_millis(void);
uint32_t port_random(void);
void     port_irq_lock(void);      /**< enter critical section (nestable) */
void     port_irq_unlock(void);

/* ---- keyer I/O ---------------------------------------------------------- */
bool port_paddle_dit(void);        /**< raw level, true = pressed          */
bool port_paddle_dah(void);
bool port_straight_key(void);
void port_key_out(bool on);        /**< transmitter keying output          */

/* ---- audio -------------------------------------------------------------- */
void port_audio_set_input_gain(int8_t db);  /**< line in PGA, -16..+30 dB */

/* ---- persistent settings ------------------------------------------------ */
bool port_settings_load(void *buf, size_t len);
bool port_settings_save(const void *buf, size_t len);

/* ---- SD card (512 byte sectors) ----------------------------------------- */
bool port_sd_present(void);
bool port_sd_init(void);
bool port_sd_read(uint32_t lba, uint8_t *buf, uint32_t count);

/* ---- USB mass storage ----------------------------------------------------- */
/** switch the USB drive on/off (off: USB FS only supplies power) */
void port_usb_drive(bool on);
/** true while the USB drive is switched on - the app must not access the SD card */
bool port_usb_active(void);
/** true when a PC has mounted (configured) the drive */
bool port_usb_host_connected(void);
/** USB drive statistics (block accesses), for diagnostics on the display */
void port_usb_stats(uint32_t *reads, uint32_t *writes, uint32_t *errors);

#endif
