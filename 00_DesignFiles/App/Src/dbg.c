#include "dbg.h"
#include "stm32f4xx_hal.h"
#include "vcp.h"

extern UART_HandleTypeDef huart1;

static void Dbg_Send(bool use_vcp, const uint8_t *data, uint16_t length)
{
  if ((data == NULL) || (length == 0U))
  {
    return;
  }
  if (use_vcp)
  {
    (void)Vcp_Write(data, length);
    return;
  }
  (void)HAL_UART_Transmit(&huart1, (uint8_t *)data, length, 50U);
}

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
  bool use_vcp = Vcp_IsUp();

  if (text == NULL)
  {
    text = "";
  }

  Dbg_Send(use_vcp, (const uint8_t *)text, Dbg_TextLen(text));
  Dbg_Send(use_vcp, newline, sizeof(newline));
}

void dbg_print1(const char *text, int32_t value)
{
  char number[16];
  uint8_t newline[2] = {'\r', '\n'};
  uint16_t number_len;
  bool use_vcp = Vcp_IsUp();

  if (text == NULL)
  {
    text = "";
  }

  number_len = Dbg_FormatI32(number, value);
  Dbg_Send(use_vcp, (const uint8_t *)text, Dbg_TextLen(text));
  Dbg_Send(use_vcp, (const uint8_t *)number, number_len);
  Dbg_Send(use_vcp, newline, sizeof(newline));
}

void dbg_print2(const char *text, int32_t value_a, int32_t value_b)
{
  char number_a[16];
  char number_b[16];
  uint8_t separator = ' ';
  uint8_t newline[2] = {'\r', '\n'};
  uint16_t len_a;
  uint16_t len_b;
  bool use_vcp = Vcp_IsUp();

  if (text == NULL)
  {
    text = "";
  }

  len_a = Dbg_FormatI32(number_a, value_a);
  len_b = Dbg_FormatI32(number_b, value_b);
  Dbg_Send(use_vcp, (const uint8_t *)text, Dbg_TextLen(text));
  Dbg_Send(use_vcp, (const uint8_t *)number_a, len_a);
  Dbg_Send(use_vcp, &separator, 1U);
  Dbg_Send(use_vcp, (const uint8_t *)number_b, len_b);
  Dbg_Send(use_vcp, newline, sizeof(newline));
}
