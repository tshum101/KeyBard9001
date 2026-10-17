#ifndef LED_H
#define LED_H

#include <stdint.h>

/* 102 SK6812MINI-E on TIM1 CH1 (PA8), DMA2 Stream1, 800 kHz.
   Wire order is GRB, high bit first. A 0 is CCR 22 and a 1 is CCR 65. */

#define LED_COUNT 102U

/* Consumer HID feature payload, with no report ID byte:
   0 command (0 or 1 selects the COM port, 2 selects solid color)
   1 solid enabled (0 or 1)
   2 red, 3 green, 4 blue (0..255)
   5 brightness (0..100). 100 maps to LED_LEVEL_PERCENT of the channel. */
#define LED_CMD_SOLID 2U
#define LED_FEATURE_LEN 6U

/* Brightness 100 is this percent of the red, green, and blue values. */
#define LED_LEVEL_PERCENT 40U

/* Report ID used only while the COM port is on. COM off has no report ID. */
#define LED_FEATURE_REPORT_ID 3U

void Led_Init(void);
void Led_Poll(void);
void Led_SetSolid(uint8_t enabled, uint8_t red, uint8_t green, uint8_t blue, uint8_t brightness);
void Led_GetSolid(uint8_t *enabled, uint8_t *red, uint8_t *green, uint8_t *blue, uint8_t *brightness);

#endif
