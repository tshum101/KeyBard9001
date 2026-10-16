#ifndef BOOT_H
#define BOOT_H

#include <stdint.h>

#define BOOT_FLASH_BASE 0x08000000UL
#define BOOT_RESERVED   0x00010000UL
#define APP_FLASH_BASE  0x08010000UL
#define APP_FLASH_END   0x080E0000UL
/* Sector 11. Same addresses as 00_DesignFiles/App/Inc/nvm_layout.h. */
#define NVM_FLASH_BASE  0x080E0000UL
#define NVM_FLASH_END   0x08100000UL

/* 1: start USB DFU. 0: the application vector is valid and BTN1 is released. */
int Boot_EnterDfu(void);
void Boot_JumpToApplication(void);

#endif
