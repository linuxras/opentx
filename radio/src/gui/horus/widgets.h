/*
 * Copyright (C) OpenTX
 *
 * Based on code named
 *   th9x - http://code.google.com/p/th9x
 *   er9x - http://code.google.com/p/er9x
 *   gruvin9x - http://code.google.com/p/gruvin9x
 *
 * License GPLv2: http://www.gnu.org/licenses/gpl-2.0.html
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

 #ifndef _WIDGETS_H_
 #define _WIDGETS_H_

#include "../../opentx.h"
#include <stdio.h>

#define OPTION_MENU_NO_FOOTER          0x01
#define OPTION_MENU_TITLE_BAR          0x02
#define OPTION_MENU_NO_SCROLLBAR       0x04

#define OPTION_SLIDER_INVERS           INVERS
#define OPTION_SLIDER_BLINK            BLINK
#define OPTION_SLIDER_VERTICAL         0x04
#define OPTION_SLIDER_EMPTY_BAR        0x08
#define OPTION_SLIDER_DBL_COLOR        0x10
#define OPTION_SLIDER_TICKS            0x20
#define OPTION_SLIDER_BIG_TICKS        0x40
#define OPTION_SLIDER_TRIM_BUTTON      0x80
#define OPTION_SLIDER_NUMBER_BUTTON    0x100
#define OPTION_SLIDER_SQUARE_BUTTON    0x200


void drawColumnHeader(const char * const * headers, uint8_t index);
void drawTopbarDatetime();
void drawStick(coord_t x, coord_t y, int16_t xval, int16_t yval);

#define BUTTON_ON   0x10
#define BUTTON_OFF  0x20
void drawButton(coord_t x, coord_t y, const char * label, LcdFlags attr);
void drawCheckBox(coord_t x, coord_t y, uint8_t value, LcdFlags attr);
void drawVerticalScrollbar(coord_t x, coord_t y, coord_t h, uint16_t offset, uint16_t count, uint8_t visible);
void drawHorizontalScrollbar(coord_t x, coord_t y, coord_t w, uint16_t offset, uint16_t count, uint8_t visible);
void drawProgressBar(const char * label);
void updateProgressBar(int num, int den);
void drawShadow(coord_t x, coord_t y, coord_t w, coord_t h);
void drawTrimSquare(coord_t x, coord_t y);
void drawHorizontalTrimPosition(coord_t x, coord_t y, int16_t dir);
void drawVerticalTrimPosition(coord_t x, coord_t y, int16_t dir);
void drawVerticalSlider(coord_t x, coord_t y, int len, int val, int min, int max, uint8_t steps, uint32_t options);
void drawHorizontalSlider(coord_t x, coord_t y, int len, int val, int min, int max, uint8_t steps, uint32_t options);
void drawSlider(coord_t x, coord_t y, int len, int val, int min, int max, uint8_t steps, uint32_t options);
#define drawStatusLine(...)

select_menu_value_t selectMenuItem(coord_t x, coord_t y, const pm_char * values, select_menu_value_t value, select_menu_value_t min, select_menu_value_t max, LcdFlags attr, evt_t event);
uint8_t editCheckBox(uint8_t value, coord_t x, coord_t y, LcdFlags attr, evt_t event);
int8_t switchMenuItem(coord_t x, coord_t y, int8_t value, LcdFlags attr, evt_t event);
#if defined(GVARS)
int16_t editGVarFieldValue(coord_t x, coord_t y, int16_t value, int16_t min, int16_t max, LcdFlags attr, uint8_t editflags, evt_t event);
#else
int16_t editGVarFieldValue(coord_t x, coord_t y, int16_t value, int16_t min, int16_t max, LcdFlags attr, evt_t event);
#endif

// Screen templates
void drawScreenTemplate(const char * title, const uint8_t * icon, uint32_t options=0);
void drawMenuTemplate(const char * title, const uint8_t * const * icons, uint32_t options=0);
void drawSplash();
void drawSleepBitmap();
void drawShutdownBitmap(uint32_t index);

// Main view standard widgets
void drawTopBar();
void drawMainPots();
void drawTrims(uint8_t flightMode);

extern uint8_t linesDisplayed;

#endif // _WIDGETS_H_
