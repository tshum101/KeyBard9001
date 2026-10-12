#include "dbg.h"
#include "stm32f4xx_hal.h"

extern UART_HandleTypeDef huart1;

static uint16_t Dbg_FormatI32(char *dst, int32_t value)
{
  char digits[10];
  uint32_t magnitude;
  uint32_t count = 0U;
  uint16_t length = 0U;

  if (value < 0)
  {
    *dst++ = '-';
    length++;
    magnitude = (value == INT32_MIN) ? 2147483648U : (uint32_t)(-value);
  }
  else
  {
    magnitude = (uint32_t)value;
  }

  do
  {
    digits[count++] = (char)('0' + (magnitude % 10U));
    magnitude /= 10U;
  } while (magnitude > 0U);

  while (count > 0U)
  {
    *dst++ = digits[--count];
    length++;
  }
  *dst = '\0';
  return length;
}

void dbg_print(const char *text, int32_t value)
{
  char number[16];
  uint8_t newline[2] = {'\r', '\n'};
  uint16_t text_len = 0U;
  uint16_t number_len;

  if (text == NULL)
  {
    text = "";
  }

  while ((text[text_len] != '\0') && (text_len < UINT16_MAX))
  {
    text_len++;
  }

  number_len = Dbg_FormatI32(number, value);
  (void)HAL_UART_Transmit(&huart1, (uint8_t *)text, text_len, 50U);
  (void)HAL_UART_Transmit(&huart1, (uint8_t *)number, number_len, 20U);
  (void)HAL_UART_Transmit(&huart1, newline, sizeof(newline), 20U);
}
