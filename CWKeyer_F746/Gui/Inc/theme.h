/**
 * @file    theme.h
 * @brief   Colours and layout constants of the UI (dark theme, amber accent).
 */
#ifndef THEME_H
#define THEME_H

#include "gfx.h"

#define COL_BG          RGB565(0x0D, 0x11, 0x17)
#define COL_BAR         RGB565(0x16, 0x1B, 0x22)
#define COL_PANEL       RGB565(0x1C, 0x23, 0x2B)
#define COL_PANEL_DOWN  RGB565(0x2A, 0x33, 0x3D)
#define COL_BORDER      RGB565(0x2A, 0x33, 0x3D)
#define COL_SIDEBAR     RGB565(0x11, 0x16, 0x1C)
#define COL_ACCENT      RGB565(0xEF, 0x9F, 0x27)
#define COL_ACCENT_DARK RGB565(0x41, 0x24, 0x02)
#define COL_TEXT        RGB565(0xE6, 0xE6, 0xE6)
#define COL_MUTED       RGB565(0x9A, 0xA4, 0xAE)
#define COL_DISABLED    RGB565(0x55, 0x5E, 0x68)
#define COL_TEXTAREA    RGB565(0x05, 0x08, 0x0B)
#define COL_RX          RGB565(0x9F, 0xE1, 0xCB)
#define COL_GREEN       RGB565(0x5D, 0xCA, 0xA5)
#define COL_OK_BG       RGB565(0x17, 0x34, 0x04)
#define COL_OK_BORDER   RGB565(0x3B, 0x6D, 0x11)
#define COL_OK_TEXT     RGB565(0xC0, 0xDD, 0x97)
#define COL_ERR_BG      RGB565(0x50, 0x13, 0x13)
#define COL_ERR_BORDER  RGB565(0xA3, 0x2D, 0x2D)
#define COL_ERR_TEXT    RGB565(0xF7, 0xC1, 0xC1)

#define BAR_H           30      /**< status bar height             */
#define PAD             8       /**< outer padding                 */
#define GAP             8       /**< gap between widgets           */
#define RADIUS          8
#define CONTENT_Y       (BAR_H)
#define CONTENT_H       (LCD_H - BAR_H)

#endif
