/**
 * @file    usbd_desc.c
 * @brief   USB device descriptors (mass storage "CW Keyer SD").
 */
#include "usbd_desc.h"
#include "usbd_conf.h"
#include "usbd_core.h"
#include "usbd_ctlreq.h"

#define USBD_VID            0x0483U    /* STMicroelectronics */
#define USBD_PID            0x572AU    /* MSC demo range */
#define USBD_LANGID         0x0409U
#define USBD_MANUFACTURER   "CW Keyer"
#define USBD_PRODUCT        "CW Keyer SD card"
#define USBD_CONFIGURATION  "MSC config"
#define USBD_INTERFACE      "MSC interface"

static uint8_t *dev_desc(USBD_SpeedTypeDef speed, uint16_t *length);
static uint8_t *langid_desc(USBD_SpeedTypeDef speed, uint16_t *length);
static uint8_t *manuf_desc(USBD_SpeedTypeDef speed, uint16_t *length);
static uint8_t *product_desc(USBD_SpeedTypeDef speed, uint16_t *length);
static uint8_t *serial_desc(USBD_SpeedTypeDef speed, uint16_t *length);
static uint8_t *config_desc(USBD_SpeedTypeDef speed, uint16_t *length);
static uint8_t *itf_desc(USBD_SpeedTypeDef speed, uint16_t *length);

USBD_DescriptorsTypeDef FS_Desc =
{
    dev_desc, langid_desc, manuf_desc, product_desc, serial_desc, config_desc, itf_desc,
};

__ALIGN_BEGIN static uint8_t s_dev_desc[USB_LEN_DEV_DESC] __ALIGN_END =
{
    0x12,                       /* bLength */
    USB_DESC_TYPE_DEVICE,       /* bDescriptorType */
    0x00, 0x02,                 /* bcdUSB 2.00 */
    0x00, 0x00, 0x00,           /* class defined per interface */
    USB_MAX_EP0_SIZE,           /* bMaxPacketSize */
    LOBYTE(USBD_VID), HIBYTE(USBD_VID),
    LOBYTE(USBD_PID), HIBYTE(USBD_PID),
    0x00, 0x02,                 /* bcdDevice 2.00 */
    USBD_IDX_MFC_STR,
    USBD_IDX_PRODUCT_STR,
    USBD_IDX_SERIAL_STR,
    USBD_MAX_NUM_CONFIGURATION
};

__ALIGN_BEGIN static uint8_t s_langid[USB_LEN_LANGID_STR_DESC] __ALIGN_END =
{
    USB_LEN_LANGID_STR_DESC, USB_DESC_TYPE_STRING, LOBYTE(USBD_LANGID), HIBYTE(USBD_LANGID)
};

__ALIGN_BEGIN static uint8_t s_str[USBD_MAX_STR_DESC_SIZ] __ALIGN_END;

static uint8_t *dev_desc(USBD_SpeedTypeDef speed, uint16_t *length)
{
    (void)speed;
    *length = sizeof(s_dev_desc);
    return s_dev_desc;
}

static uint8_t *langid_desc(USBD_SpeedTypeDef speed, uint16_t *length)
{
    (void)speed;
    *length = sizeof(s_langid);
    return s_langid;
}

static uint8_t *string(const char *s, uint16_t *length)
{
    USBD_GetString((uint8_t *)s, s_str, length);
    return s_str;
}

static uint8_t *manuf_desc(USBD_SpeedTypeDef speed, uint16_t *length)   { (void)speed; return string(USBD_MANUFACTURER, length); }
static uint8_t *product_desc(USBD_SpeedTypeDef speed, uint16_t *length) { (void)speed; return string(USBD_PRODUCT, length); }
static uint8_t *config_desc(USBD_SpeedTypeDef speed, uint16_t *length)  { (void)speed; return string(USBD_CONFIGURATION, length); }
static uint8_t *itf_desc(USBD_SpeedTypeDef speed, uint16_t *length)     { (void)speed; return string(USBD_INTERFACE, length); }

/* serial number from the 96 bit unique ID */
static uint8_t *serial_desc(USBD_SpeedTypeDef speed, uint16_t *length)
{
    (void)speed;
    static char sn[13];
    uint32_t id = *(volatile uint32_t *)(UID_BASE) + *(volatile uint32_t *)(UID_BASE + 8U);
    uint32_t id2 = *(volatile uint32_t *)(UID_BASE + 4U);
    static const char hex[] = "0123456789ABCDEF";
    for (int i = 0; i < 8; i++) sn[i] = hex[(id >> (28 - 4 * i)) & 0xFU];
    for (int i = 0; i < 4; i++) sn[8 + i] = hex[(id2 >> (12 - 4 * i)) & 0xFU];
    sn[12] = 0;
    return string(sn, length);
}
