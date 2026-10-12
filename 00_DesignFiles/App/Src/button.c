#include "button.h"

static bool Button_IsRawPressed(const Button *btn)
{
  GPIO_PinState level = HAL_GPIO_ReadPin(btn->GPIO_PORT, btn->GPIO_PIN);

  if (btn->active_low)
  {
    return level == GPIO_PIN_RESET;
  }
  return level == GPIO_PIN_SET;
}

void Button_Init(Button *btn)
{
  bool pressed = Button_IsRawPressed(btn);

  btn->stable_pressed = pressed;
  btn->debounce_raw = pressed;
  btn->hold_latched = false;
  btn->debounce_start_ms = HAL_GetTick();
  btn->press_start_ms = btn->debounce_start_ms;
}

void Button_Update(Button *btn)
{
  uint32_t now = HAL_GetTick();
  bool raw = Button_IsRawPressed(btn);

  if (raw != btn->debounce_raw)
  {
    btn->debounce_raw = raw;
    btn->debounce_start_ms = now;
  }

  if ((raw != btn->stable_pressed) &&
      ((int32_t)(now - btn->debounce_start_ms) >= (int32_t)btn->debounce_time_ms))
  {
    btn->stable_pressed = raw;
    if (raw)
    {
      btn->pressed_flag = true;
      btn->press_start_ms = now;
      btn->hold_latched = false;
    }
    else
    {
      btn->released_flag = true;
    }
  }

  if (btn->stable_pressed && (btn->hold_latched == false) && (btn->hold_time_ms > 0U) &&
      ((int32_t)(now - btn->press_start_ms) >= (int32_t)btn->hold_time_ms))
  {
    btn->held_flag = true;
    btn->hold_latched = true;
  }
}
