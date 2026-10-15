#ifndef MATRIX_H
#define MATRIX_H

#include <stdint.h>

void Matrix_Init(void);
void Matrix_OnTick(void);
void Matrix_Poll(void);

/* Writes an 8-byte boot keyboard report.
   Byte 0 is the modifier bitfield. Bytes 2 through 7 are key codes. */
void Matrix_FillReport(uint8_t report[8]);

/* Bit 0 mute, bit 1 volume down, bit 2 volume up. */
void Matrix_FillConsumer(uint8_t report[1]);

#endif
