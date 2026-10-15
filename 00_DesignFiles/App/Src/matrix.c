#include "matrix.h"

#include <stdbool.h>
#include "button.h"
#include "dbg.h"
#include "main.h"

#define MATRIX_ROWS 6U
#define MATRIX_COLS 19U
#define MATRIX_DEBOUNCE_MS 10U
#define MATRIX_HOLD_MS 1000U
#define MATRIX_REPORT_LEN 8U
#define MATRIX_USAGE_ERR_ROLLOVER 0x01U

/* Modifier bits in boot report byte 0. */
#define MATRIX_MOD_LCTRL 0x01U
#define MATRIX_MOD_LSHIFT 0x02U
#define MATRIX_MOD_LALT 0x04U
#define MATRIX_MOD_LGUI 0x08U
#define MATRIX_MOD_RCTRL 0x10U
#define MATRIX_MOD_RSHIFT 0x20U
#define MATRIX_MOD_RALT 0x40U

/* Consumer report bits. Order matches the volume HID descriptor. */
#define MATRIX_CONS_MUTE 0x01U
#define MATRIX_CONS_VOL_DOWN 0x02U
#define MATRIX_CONS_VOL_UP 0x04U
#define MATRIX_CONS_CALC 0x08U

#define MATRIX_KIND_KEY 0U
#define MATRIX_KIND_MOD 1U
#define MATRIX_KIND_CONS 2U
#define MATRIX_KIND_LOCAL 3U

/* ~20 us at 72 MHz once optimized. Column pull-ups need this after the row falls. */
#define MATRIX_SETTLE_LOOPS 480U

#define MATRIX_BTN_INIT \
  { .debounce_time_ms = MATRIX_DEBOUNCE_MS, .hold_time_ms = MATRIX_HOLD_MS }

/* row and col are 0-based. R1 C1 is 0,0. */
#define MATRIX_MAP(row, col, kind, code, name) \
  { (row), (col), (kind), (code), (name), MATRIX_BTN_INIT }

typedef struct
{
  GPIO_TypeDef *port;
  uint16_t pin;
} MatrixPin;

typedef struct
{
  uint8_t row;
  uint8_t col;
  uint8_t kind;
  uint8_t code;
  const char *name;
  Button button;
} MatrixKey;

static const MatrixPin k_rows[MATRIX_ROWS] = {
  { R1_GPIO_Port, R1_Pin },
  { R2_GPIO_Port, R2_Pin },
  { R3_GPIO_Port, R3_Pin },
  { R4_GPIO_Port, R4_Pin },
  { R5_GPIO_Port, R5_Pin },
  { R6_GPIO_Port, R6_Pin },
};

static const MatrixPin k_columns[MATRIX_COLS] = {
  { C1_GPIO_Port, C1_Pin },
  { C2_GPIO_Port, C2_Pin },
  { C3_GPIO_Port, C3_Pin },
  { C4_GPIO_Port, C4_Pin },
  { C5_GPIO_Port, C5_Pin },
  { C6_GPIO_Port, C6_Pin },
  { C7_GPIO_Port, C7_Pin },
  { C8_GPIO_Port, C8_Pin },
  { C9_GPIO_Port, C9_Pin },
  { C10_GPIO_Port, C10_Pin },
  { C11_GPIO_Port, C11_Pin },
  { C12_GPIO_Port, C12_Pin },
  { C13_GPIO_Port, C13_Pin },
  { C14_GPIO_Port, C14_Pin },
  { C15_GPIO_Port, C15_Pin },
  { C16_GPIO_Port, C16_Pin },
  { C17_GPIO_Port, C17_Pin },
  { C18_GPIO_Port, C18_Pin },
  { C19_GPIO_Port, C19_Pin },
};

/* Shifted legends are produced by the OS from the base usage plus a shift bit.
   Fn and Keybard are debounced for later features and are left out of the HID reports. */
static MatrixKey s_keys[] = {
  /* R1 */
  MATRIX_MAP(0U, 0U, MATRIX_KIND_KEY, 0x29U, "esc"),
  MATRIX_MAP(0U, 2U, MATRIX_KIND_KEY, 0x3AU, "f1"),
  MATRIX_MAP(0U, 3U, MATRIX_KIND_KEY, 0x3BU, "f2"),
  MATRIX_MAP(0U, 4U, MATRIX_KIND_KEY, 0x3CU, "f3"),
  MATRIX_MAP(0U, 5U, MATRIX_KIND_KEY, 0x3DU, "f4"),
  MATRIX_MAP(0U, 6U, MATRIX_KIND_KEY, 0x3EU, "f5"),
  MATRIX_MAP(0U, 7U, MATRIX_KIND_KEY, 0x3FU, "f6"),
  MATRIX_MAP(0U, 8U, MATRIX_KIND_KEY, 0x40U, "f7"),
  MATRIX_MAP(0U, 9U, MATRIX_KIND_KEY, 0x41U, "f8"),
  MATRIX_MAP(0U, 10U, MATRIX_KIND_KEY, 0x42U, "f9"),
  MATRIX_MAP(0U, 11U, MATRIX_KIND_KEY, 0x43U, "f10"),
  MATRIX_MAP(0U, 12U, MATRIX_KIND_KEY, 0x44U, "f11"),
  MATRIX_MAP(0U, 13U, MATRIX_KIND_KEY, 0x45U, "f12"),
  MATRIX_MAP(0U, 14U, MATRIX_KIND_CONS, MATRIX_CONS_CALC, "calc"),
  MATRIX_MAP(0U, 15U, MATRIX_KIND_CONS, MATRIX_CONS_MUTE, "mute"),
  MATRIX_MAP(0U, 16U, MATRIX_KIND_CONS, MATRIX_CONS_VOL_DOWN, "voldown"),
  MATRIX_MAP(0U, 17U, MATRIX_KIND_CONS, MATRIX_CONS_VOL_UP, "volup"),
  MATRIX_MAP(0U, 18U, MATRIX_KIND_LOCAL, 0x00U, "keybard"),

  /* R2 */
  MATRIX_MAP(1U, 0U, MATRIX_KIND_KEY, 0x35U, "grave"),
  MATRIX_MAP(1U, 1U, MATRIX_KIND_KEY, 0x1EU, "1"),
  MATRIX_MAP(1U, 2U, MATRIX_KIND_KEY, 0x1FU, "2"),
  MATRIX_MAP(1U, 3U, MATRIX_KIND_KEY, 0x20U, "3"),
  MATRIX_MAP(1U, 4U, MATRIX_KIND_KEY, 0x21U, "4"),
  MATRIX_MAP(1U, 5U, MATRIX_KIND_KEY, 0x22U, "5"),
  MATRIX_MAP(1U, 6U, MATRIX_KIND_KEY, 0x23U, "6"),
  MATRIX_MAP(1U, 7U, MATRIX_KIND_KEY, 0x24U, "7"),
  MATRIX_MAP(1U, 8U, MATRIX_KIND_KEY, 0x25U, "8"),
  MATRIX_MAP(1U, 9U, MATRIX_KIND_KEY, 0x26U, "9"),
  MATRIX_MAP(1U, 10U, MATRIX_KIND_KEY, 0x27U, "0"),
  MATRIX_MAP(1U, 11U, MATRIX_KIND_KEY, 0x2DU, "minus"),
  MATRIX_MAP(1U, 12U, MATRIX_KIND_KEY, 0x2EU, "equal"),
  MATRIX_MAP(1U, 13U, MATRIX_KIND_KEY, 0x2AU, "backspace"),
  MATRIX_MAP(1U, 14U, MATRIX_KIND_KEY, 0x4CU, "delete"),
  MATRIX_MAP(1U, 15U, MATRIX_KIND_KEY, 0x53U, "numlock"),
  MATRIX_MAP(1U, 16U, MATRIX_KIND_KEY, 0x54U, "kpdiv"),
  MATRIX_MAP(1U, 17U, MATRIX_KIND_KEY, 0x55U, "kpmul"),
  MATRIX_MAP(1U, 18U, MATRIX_KIND_KEY, 0x56U, "kpmin"),

  /* R3 */
  MATRIX_MAP(2U, 0U, MATRIX_KIND_KEY, 0x2BU, "tab"),
  MATRIX_MAP(2U, 1U, MATRIX_KIND_KEY, 0x14U, "q"),
  MATRIX_MAP(2U, 2U, MATRIX_KIND_KEY, 0x1AU, "w"),
  MATRIX_MAP(2U, 3U, MATRIX_KIND_KEY, 0x08U, "e"),
  MATRIX_MAP(2U, 4U, MATRIX_KIND_KEY, 0x15U, "r"),
  MATRIX_MAP(2U, 5U, MATRIX_KIND_KEY, 0x17U, "t"),
  MATRIX_MAP(2U, 6U, MATRIX_KIND_KEY, 0x1CU, "y"),
  MATRIX_MAP(2U, 7U, MATRIX_KIND_KEY, 0x18U, "u"),
  MATRIX_MAP(2U, 8U, MATRIX_KIND_KEY, 0x0CU, "i"),
  MATRIX_MAP(2U, 9U, MATRIX_KIND_KEY, 0x12U, "o"),
  MATRIX_MAP(2U, 10U, MATRIX_KIND_KEY, 0x13U, "p"),
  MATRIX_MAP(2U, 11U, MATRIX_KIND_KEY, 0x2FU, "lbrack"),
  MATRIX_MAP(2U, 12U, MATRIX_KIND_KEY, 0x30U, "rbrack"),
  MATRIX_MAP(2U, 13U, MATRIX_KIND_KEY, 0x31U, "bslash"),
  MATRIX_MAP(2U, 14U, MATRIX_KIND_KEY, 0x4AU, "home"),
  MATRIX_MAP(2U, 15U, MATRIX_KIND_KEY, 0x5FU, "kp7"),
  MATRIX_MAP(2U, 16U, MATRIX_KIND_KEY, 0x60U, "kp8"),
  MATRIX_MAP(2U, 17U, MATRIX_KIND_KEY, 0x61U, "kp9"),
  MATRIX_MAP(2U, 18U, MATRIX_KIND_KEY, 0x57U, "kpplus"),

  /* R4 */
  MATRIX_MAP(3U, 0U, MATRIX_KIND_KEY, 0x39U, "caps"),
  MATRIX_MAP(3U, 1U, MATRIX_KIND_KEY, 0x04U, "a"),
  MATRIX_MAP(3U, 2U, MATRIX_KIND_KEY, 0x16U, "s"),
  MATRIX_MAP(3U, 3U, MATRIX_KIND_KEY, 0x07U, "d"),
  MATRIX_MAP(3U, 4U, MATRIX_KIND_KEY, 0x09U, "f"),
  MATRIX_MAP(3U, 5U, MATRIX_KIND_KEY, 0x0AU, "g"),
  MATRIX_MAP(3U, 6U, MATRIX_KIND_KEY, 0x0BU, "h"),
  MATRIX_MAP(3U, 7U, MATRIX_KIND_KEY, 0x0DU, "j"),
  MATRIX_MAP(3U, 8U, MATRIX_KIND_KEY, 0x0EU, "k"),
  MATRIX_MAP(3U, 9U, MATRIX_KIND_KEY, 0x0FU, "l"),
  MATRIX_MAP(3U, 10U, MATRIX_KIND_KEY, 0x33U, "scolon"),
  MATRIX_MAP(3U, 11U, MATRIX_KIND_KEY, 0x34U, "quote"),
  MATRIX_MAP(3U, 13U, MATRIX_KIND_KEY, 0x28U, "enter"),
  MATRIX_MAP(3U, 14U, MATRIX_KIND_KEY, 0x4DU, "end"),
  MATRIX_MAP(3U, 15U, MATRIX_KIND_KEY, 0x5CU, "kp4"),
  MATRIX_MAP(3U, 16U, MATRIX_KIND_KEY, 0x5DU, "kp5"),
  MATRIX_MAP(3U, 17U, MATRIX_KIND_KEY, 0x5EU, "kp6"),

  /* R5 */
  MATRIX_MAP(4U, 0U, MATRIX_KIND_MOD, MATRIX_MOD_LSHIFT, "lshift"),
  MATRIX_MAP(4U, 1U, MATRIX_KIND_KEY, 0x1DU, "z"),
  MATRIX_MAP(4U, 2U, MATRIX_KIND_KEY, 0x1BU, "x"),
  MATRIX_MAP(4U, 3U, MATRIX_KIND_KEY, 0x06U, "c"),
  MATRIX_MAP(4U, 4U, MATRIX_KIND_KEY, 0x19U, "v"),
  MATRIX_MAP(4U, 5U, MATRIX_KIND_KEY, 0x05U, "b"),
  MATRIX_MAP(4U, 6U, MATRIX_KIND_KEY, 0x11U, "n"),
  MATRIX_MAP(4U, 7U, MATRIX_KIND_KEY, 0x10U, "m"),
  MATRIX_MAP(4U, 8U, MATRIX_KIND_KEY, 0x36U, "comma"),
  MATRIX_MAP(4U, 9U, MATRIX_KIND_KEY, 0x37U, "dot"),
  MATRIX_MAP(4U, 10U, MATRIX_KIND_KEY, 0x38U, "slash"),
  MATRIX_MAP(4U, 11U, MATRIX_KIND_MOD, MATRIX_MOD_RSHIFT, "rshift"),
  MATRIX_MAP(4U, 14U, MATRIX_KIND_KEY, 0x52U, "up"),
  MATRIX_MAP(4U, 15U, MATRIX_KIND_KEY, 0x59U, "kp1"),
  MATRIX_MAP(4U, 16U, MATRIX_KIND_KEY, 0x5AU, "kp2"),
  MATRIX_MAP(4U, 17U, MATRIX_KIND_KEY, 0x5BU, "kp3"),
  MATRIX_MAP(4U, 18U, MATRIX_KIND_KEY, 0x58U, "kpenter"),

  /* R6 */
  MATRIX_MAP(5U, 0U, MATRIX_KIND_MOD, MATRIX_MOD_LCTRL, "lctrl"),
  MATRIX_MAP(5U, 1U, MATRIX_KIND_MOD, MATRIX_MOD_LGUI, "win"),
  MATRIX_MAP(5U, 2U, MATRIX_KIND_MOD, MATRIX_MOD_LALT, "lalt"),
  MATRIX_MAP(5U, 3U, MATRIX_KIND_KEY, 0x2CU, "space"),
  MATRIX_MAP(5U, 9U, MATRIX_KIND_MOD, MATRIX_MOD_RALT, "ralt"),
  MATRIX_MAP(5U, 10U, MATRIX_KIND_LOCAL, 0x00U, "fn"),
  MATRIX_MAP(5U, 11U, MATRIX_KIND_MOD, MATRIX_MOD_RCTRL, "rctrl"),
  MATRIX_MAP(5U, 13U, MATRIX_KIND_KEY, 0x50U, "left"),
  MATRIX_MAP(5U, 14U, MATRIX_KIND_KEY, 0x51U, "down"),
  MATRIX_MAP(5U, 15U, MATRIX_KIND_KEY, 0x4FU, "right"),
  MATRIX_MAP(5U, 16U, MATRIX_KIND_KEY, 0x62U, "kp0"),
  MATRIX_MAP(5U, 17U, MATRIX_KIND_KEY, 0x63U, "kpdot"),
};

_Static_assert((sizeof(s_keys) / sizeof(s_keys[0])) == 102U, "layout key count");

static volatile uint32_t s_raw[MATRIX_ROWS];
static volatile uint32_t s_scan_count;
static uint32_t s_seen_count;

static void Matrix_Settle(void)
{
  uint32_t i;

  for (i = 0U; i < MATRIX_SETTLE_LOOPS; i++)
  {
    __NOP();
  }
}

/* Every column is on GPIOE, GPIOB, or GPIOD. */
static uint32_t Matrix_PortLevel(GPIO_TypeDef *port, uint32_t idr_e, uint32_t idr_b, uint32_t idr_d)
{
  if (port == GPIOE)
  {
    return idr_e;
  }
  if (port == GPIOB)
  {
    return idr_b;
  }
  return idr_d;
}

static void Matrix_Scan(uint32_t raw[MATRIX_ROWS])
{
  uint8_t row;
  uint8_t col;

  for (row = 0U; row < MATRIX_ROWS; row++)
  {
    uint32_t bits = 0U;
    uint32_t idr_e;
    uint32_t idr_b;
    uint32_t idr_d;

    HAL_GPIO_WritePin(k_rows[row].port, k_rows[row].pin, GPIO_PIN_RESET);
    Matrix_Settle();
    idr_e = GPIOE->IDR;
    idr_b = GPIOB->IDR;
    idr_d = GPIOD->IDR;
    HAL_GPIO_WritePin(k_rows[row].port, k_rows[row].pin, GPIO_PIN_SET);

    for (col = 0U; col < MATRIX_COLS; col++)
    {
      uint32_t level = Matrix_PortLevel(k_columns[col].port, idr_e, idr_b, idr_d);

      if ((level & k_columns[col].pin) == 0U)
      {
        bits |= (1UL << col);
      }
    }
    raw[row] = bits;
  }
}

static uint8_t Matrix_KeyCount(void)
{
  return (uint8_t)(sizeof(s_keys) / sizeof(s_keys[0]));
}

static bool Matrix_RawBit(const uint32_t raw[MATRIX_ROWS], uint8_t row, uint8_t col)
{
  return (raw[row] & (1UL << col)) != 0U;
}

static void Matrix_Log(const char *name, const char *event)
{
#if dbgKEYSTROKES
  char line[48];
  uint8_t at = 0U;
  uint8_t event_at = 0U;

  if (name == NULL)
  {
    name = "";
  }
  if (event == NULL)
  {
    event = "";
  }

  while ((name[at] != '\0') && (at < 24U))
  {
    line[at] = name[at];
    at++;
  }
  if (at < (sizeof(line) - 1U))
  {
    line[at] = ' ';
    at++;
  }
  while ((event[event_at] != '\0') && (at < (sizeof(line) - 1U)))
  {
    line[at] = event[event_at];
    at++;
    event_at++;
  }
  line[at] = '\0';
  dbg_print_text(line);
#else
  (void)name;
  (void)event;
#endif
}

void Matrix_Init(void)
{
  uint32_t raw[MATRIX_ROWS];
  uint8_t i;

  Matrix_Scan(raw);
  for (i = 0U; i < Matrix_KeyCount(); i++)
  {
    Button_InitLevel(&s_keys[i].button, Matrix_RawBit(raw, s_keys[i].row, s_keys[i].col));
  }
}

void Matrix_OnTick(void)
{
  uint32_t raw[MATRIX_ROWS];
  uint8_t row;

  Matrix_Scan(raw);
  for (row = 0U; row < MATRIX_ROWS; row++)
  {
    s_raw[row] = raw[row];
  }
  s_scan_count++;
}

void Matrix_Poll(void)
{
  uint32_t raw[MATRIX_ROWS];
  uint32_t count;
  uint32_t primask;
  uint8_t row;
  uint8_t i;

  primask = __get_PRIMASK();
  __disable_irq();
  count = s_scan_count;
  for (row = 0U; row < MATRIX_ROWS; row++)
  {
    raw[row] = s_raw[row];
  }
  __set_PRIMASK(primask);

  if (count == s_seen_count)
  {
    return;
  }
  s_seen_count = count;

  for (i = 0U; i < Matrix_KeyCount(); i++)
  {
    MatrixKey *key = &s_keys[i];

    Button_UpdateLevel(&key->button, Matrix_RawBit(raw, key->row, key->col));
    if (key->button.pressed_flag)
    {
      key->button.pressed_flag = false;
      Matrix_Log(key->name, "pressed");
    }
    if (key->button.released_flag)
    {
      key->button.released_flag = false;
      Matrix_Log(key->name, "released");
    }
    if (key->button.held_flag)
    {
      key->button.held_flag = false;
      Matrix_Log(key->name, "held");
    }
  }
}

void Matrix_FillReport(uint8_t report[8])
{
  uint8_t slot = 0U;
  bool overflow = false;
  uint8_t i;

  for (i = 0U; i < MATRIX_REPORT_LEN; i++)
  {
    report[i] = 0U;
  }

  for (i = 0U; i < Matrix_KeyCount(); i++)
  {
    const MatrixKey *key = &s_keys[i];

    if (Button_IsPressed(&key->button) == false)
    {
      continue;
    }

    if (key->kind == MATRIX_KIND_MOD)
    {
      report[0] = (uint8_t)(report[0] | key->code);
    }
    else if (key->kind == MATRIX_KIND_KEY)
    {
      if (slot < 6U)
      {
        report[2U + slot] = key->code;
        slot++;
      }
      else
      {
        overflow = true;
      }
    }
  }

  if (overflow)
  {
    for (slot = 0U; slot < 6U; slot++)
    {
      report[2U + slot] = MATRIX_USAGE_ERR_ROLLOVER;
    }
  }
}

void Matrix_FillConsumer(uint8_t report[1])
{
  uint8_t i;

  report[0] = 0U;
  for (i = 0U; i < Matrix_KeyCount(); i++)
  {
    const MatrixKey *key = &s_keys[i];

    if ((key->kind == MATRIX_KIND_CONS) && (Button_IsPressed(&key->button) == true))
    {
      report[0] = (uint8_t)(report[0] | key->code);
    }
  }
}
