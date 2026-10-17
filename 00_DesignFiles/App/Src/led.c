#include "led.h"

#include "main.h"

/* One PWM period is 1.25 us (ARR 89 at 72 MHz). The datasheet reset low
   is longer than 200 us, so the frame ends with 250 us of CCR 0.
   The buffer stays in SRAM. DMA2 cannot read CCM. */

#define LED_DUTY_0 22U
#define LED_DUTY_1 65U
#define LED_RESET_SLOTS 200U
#define LED_FRAME_SLOTS ((LED_COUNT * 24U) + LED_RESET_SLOTS)

_Static_assert(LED_DUTY_0 <= 89U, "SK6812 zero duty exceeds ARR");
_Static_assert(LED_DUTY_1 <= 89U, "SK6812 one duty exceeds ARR");
_Static_assert(LED_FRAME_SLOTS <= 65535U, "LED DMA length");

extern TIM_HandleTypeDef htim1;

static uint16_t s_frame[LED_FRAME_SLOTS];
static volatile uint8_t s_enabled;
static volatile uint8_t s_red;
static volatile uint8_t s_green;
static volatile uint8_t s_blue;
static volatile uint8_t s_brightness;
static volatile uint16_t s_gen;
static volatile uint8_t s_busy;
static uint16_t s_applied;

static uint8_t Led_Scale(uint8_t channel, uint8_t brightness)
{
  return (uint8_t)(((uint32_t)channel * (uint32_t)brightness * (uint32_t)LED_LEVEL_PERCENT) / 10000U);
}

static void Led_Encode(uint8_t enabled, uint8_t red, uint8_t green, uint8_t blue, uint8_t brightness)
{
  uint8_t channels[3];
  uint16_t index = 0U;
  uint8_t color;

  if (enabled != 0U)
  {
    channels[0] = Led_Scale(green, brightness);
    channels[1] = Led_Scale(red, brightness);
    channels[2] = Led_Scale(blue, brightness);
  }
  else
  {
    channels[0] = 0U;
    channels[1] = 0U;
    channels[2] = 0U;
  }

  for (color = 0U; color < 3U; color++)
  {
    uint8_t bit;
    uint8_t value = channels[color];

    for (bit = 0U; bit < 8U; bit++)
    {
      uint8_t mask = (uint8_t)(0x80U >> bit);
      s_frame[index++] = (value & mask) != 0U ? LED_DUTY_1 : LED_DUTY_0;
    }
  }

  /* Every LED shows the same color in solid mode. */
  {
    uint16_t led;
    uint16_t first = 0U;

    for (led = 1U; led < LED_COUNT; led++)
    {
      uint16_t slot;

      for (slot = 0U; slot < 24U; slot++)
      {
        s_frame[index++] = s_frame[first++];
      }
    }
  }

  while (index < LED_FRAME_SLOTS)
  {
    s_frame[index++] = 0U;
  }
}

static void Led_Finish(TIM_HandleTypeDef *htim)
{
  if ((htim == NULL) || (htim->Instance != TIM1))
  {
    return;
  }
  __HAL_TIM_DISABLE_DMA(htim, TIM_DMA_CC1);
  __HAL_TIM_SET_COMPARE(htim, TIM_CHANNEL_1, 0U);
  s_busy = 0U;
}

void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim)
{
  Led_Finish(htim);
}

void HAL_TIM_ErrorCallback(TIM_HandleTypeDef *htim)
{
  Led_Finish(htim);
}

void Led_Init(void)
{
  s_enabled = 0U;
  s_red = 0U;
  s_green = 0U;
  s_blue = 0U;
  s_brightness = 0U;
  s_gen = 1U;
  s_applied = 0U;
  s_busy = 0U;
}

void Led_SetSolid(uint8_t enabled, uint8_t red, uint8_t green, uint8_t blue, uint8_t brightness)
{
  if (brightness > 100U)
  {
    brightness = 100U;
  }
  s_enabled = enabled != 0U ? 1U : 0U;
  s_red = red;
  s_green = green;
  s_blue = blue;
  s_brightness = brightness;
  s_gen++;
}

void Led_GetSolid(uint8_t *enabled, uint8_t *red, uint8_t *green, uint8_t *blue, uint8_t *brightness)
{
  if (enabled != NULL)
  {
    *enabled = s_enabled;
  }
  if (red != NULL)
  {
    *red = s_red;
  }
  if (green != NULL)
  {
    *green = s_green;
  }
  if (blue != NULL)
  {
    *blue = s_blue;
  }
  if (brightness != NULL)
  {
    *brightness = s_brightness;
  }
}

void Led_Poll(void)
{
  uint8_t enabled;
  uint8_t red;
  uint8_t green;
  uint8_t blue;
  uint8_t brightness;
  uint16_t gen;

  if (s_busy != 0U)
  {
    return;
  }
  gen = s_gen;
  if (gen == s_applied)
  {
    return;
  }

  enabled = s_enabled;
  red = s_red;
  green = s_green;
  blue = s_blue;
  brightness = s_brightness;
  if (gen != s_gen)
  {
    return;
  }

  Led_Encode(enabled, red, green, blue, brightness);
  s_busy = 1U;
  if (HAL_TIM_PWM_Start_DMA(&htim1, TIM_CHANNEL_1, (const uint32_t *)s_frame, LED_FRAME_SLOTS) != HAL_OK)
  {
    (void)HAL_TIM_PWM_Stop_DMA(&htim1, TIM_CHANNEL_1);
    s_busy = 0U;
    return;
  }
  s_applied = gen;
}
