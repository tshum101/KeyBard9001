#ifndef DFU_FLASH_H
#define DFU_FLASH_H

#include <stdint.h>

#define DFU_FLASH_DESC "@Internal Flash   /0x08010000/01*064Kg,06*128Kg"

int DfuFlash_Erase(uint32_t address);
int DfuFlash_Write(uint32_t address, const uint8_t *data, uint32_t length);
int DfuFlash_Read(uint32_t address, uint8_t *dest, uint32_t length);

#endif
