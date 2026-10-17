#ifndef DBG_H
#define DBG_H

#include <stdbool.h>
#include <stdint.h>

/* Matrix key press, release, and hold lines. Set to true to enable. */
#define dbgKEYSTROKES true

/* Each print ends with CR LF. While the COM port is up the line goes out
   that port. Otherwise it goes out USART1. */

void dbg_print_text(const char *text);
void dbg_print1(const char *text, int32_t value);
void dbg_print2(const char *text, int32_t value_a, int32_t value_b);

#endif
