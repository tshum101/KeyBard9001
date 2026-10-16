#ifndef NVM_LAYOUT_H
#define NVM_LAYOUT_H

#include <stdint.h>

/* Sector 11 of the STM32F407VG. Erase size is the whole sector, so the
   firmware image stops at the sector below this. DFU refuses this range.
   Same addresses as 02_Bootloader/Boot/Inc/boot.h. */
#define NVM_FLASH_BASE 0x080E0000UL
#define NVM_FLASH_SIZE 0x00020000UL

#endif
