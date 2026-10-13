#ifndef DBG_H
#define DBG_H

#include <stdint.h>

void dbg_print_text(const char *text);
void dbg_print1(const char *text, int32_t value);
void dbg_print2(const char *text, int32_t value_a, int32_t value_b);

#endif
