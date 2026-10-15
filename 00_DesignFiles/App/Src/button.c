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

static void Button_AcceptRaw(Button *btn, bool raw, uint32_t now)
{
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

void Button_InitLevel(Button *btn, bool raw_pressed)
{
  btn->stable_pressed = raw_pressed;
  btn->debounce_raw = raw_pressed;
  btn->hold_latched = false;
  btn->debounce_start_ms = HAL_GetTick();
  btn->press_start_ms = btn->debounce_start_ms;
}

void Button_Init(Button *btn)
{
  Button_InitLevel(btn, Button_IsRawPressed(btn));
}

void Button_UpdateLevel(Button *btn, bool raw_pressed)
{
  Button_AcceptRaw(btn, raw_pressed, HAL_GetTick());
}

void Button_Update(Button *btn)
{
  Button_UpdateLevel(btn, Button_IsRawPressed(btn));
}

bool Button_IsPressed(const Button *btn)
{
  return btn->stable_pressed;
}
