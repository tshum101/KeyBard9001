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

static uint16_t Dbg_TextLen(const char *text)
{
  uint16_t text_len = 0U;

  while ((text[text_len] != '\0') && (text_len < UINT16_MAX))
  {
    text_len++;
  }
  return text_len;
}

void dbg_print_text(const char *text)
{
  uint8_t newline[2] = {'\r', '\n'};

  if (text == NULL)
  {
    text = "";
  }

  (void)HAL_UART_Transmit(&huart1, (uint8_t *)text, Dbg_TextLen(text), 50U);
  (void)HAL_UART_Transmit(&huart1, newline, sizeof(newline), 20U);
}

void dbg_print1(const char *text, int32_t value)
{
  char number[16];
  uint8_t newline[2] = {'\r', '\n'};
  uint16_t number_len;

  if (text == NULL)
  {
    text = "";
  }

  number_len = Dbg_FormatI32(number, value);
  (void)HAL_UART_Transmit(&huart1, (uint8_t *)text, Dbg_TextLen(text), 50U);
  (void)HAL_UART_Transmit(&huart1, (uint8_t *)number, number_len, 20U);
  (void)HAL_UART_Transmit(&huart1, newline, sizeof(newline), 20U);
}

void dbg_print2(const char *text, int32_t value_a, int32_t value_b)
{
  char number_a[16];
  char number_b[16];
  uint8_t separator = ' ';
  uint8_t newline[2] = {'\r', '\n'};
  uint16_t len_a;
  uint16_t len_b;

  if (text == NULL)
  {
    text = "";
  }

  len_a = Dbg_FormatI32(number_a, value_a);
  len_b = Dbg_FormatI32(number_b, value_b);
  (void)HAL_UART_Transmit(&huart1, (uint8_t *)text, Dbg_TextLen(text), 50U);
  (void)HAL_UART_Transmit(&huart1, (uint8_t *)number_a, len_a, 20U);
  (void)HAL_UART_Transmit(&huart1, &separator, 1U, 20U);
  (void)HAL_UART_Transmit(&huart1, (uint8_t *)number_b, len_b, 20U);
  (void)HAL_UART_Transmit(&huart1, newline, sizeof(newline), 20U);
}
