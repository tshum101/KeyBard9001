#ifndef BUTTON_H
#define BUTTON_H

#include <stdbool.h>
#include <stdint.h>
#include "stm32f4xx_hal.h"

typedef struct
{
  GPIO_TypeDef *GPIO_PORT;
  uint16_t GPIO_PIN;
  uint32_t debounce_time_ms;
  uint32_t hold_time_ms;
  bool active_low;
  bool pressed_flag;
  bool released_flag;
  bool held_flag;

  /* Internal. Callers do not read or write these. */
  bool stable_pressed;
  bool debounce_raw;
  bool hold_latched;
  uint32_t debounce_start_ms;
  uint32_t press_start_ms;
} Button;

void Button_Init(Button *btn);
void Button_Update(Button *btn);

#endif
