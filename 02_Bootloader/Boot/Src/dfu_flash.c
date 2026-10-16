#include "dfu_flash.h"

#include "boot.h"
#include "stm32f4xx_hal.h"

#include <string.h>

static int range_ok(uint32_t address, uint32_t length)
{
  if (length == 0U)
  {
    return 0;
  }
  if (address < APP_FLASH_BASE)
  {
    return 0;
  }
  if ((address + length) < address)
  {
    return 0;
  }
  if ((address + length) > APP_FLASH_END)
  {
    return 0;
  }
  return 1;
}

static int sector_of(uint32_t address, uint32_t *sector)
{
  if ((address < APP_FLASH_BASE) || (address >= APP_FLASH_END))
  {
    return 0;
  }
  if (address < 0x08020000UL)
  {
    *sector = FLASH_SECTOR_4;
    return 1;
  }
  *sector = FLASH_SECTOR_5 + ((address - 0x08020000UL) / 0x00020000UL);
  return 1;
}

static void clear_flash_flags(void)
{
  __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR |
                         FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);
}

int DfuFlash_Erase(uint32_t address)
{
  FLASH_EraseInitTypeDef erase = {0};
  uint32_t sector = 0U;
  uint32_t error = 0U;

  if (sector_of(address, &sector) == 0)
  {
    return 0;
  }
  HAL_FLASH_Unlock();
  clear_flash_flags();
  erase.TypeErase = FLASH_TYPEERASE_SECTORS;
  erase.Sector = sector;
  erase.NbSectors = 1U;
  erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;
  if (HAL_FLASHEx_Erase(&erase, &error) != HAL_OK)
  {
    HAL_FLASH_Lock();
    return 0;
  }
  HAL_FLASH_Lock();
  return 1;
}

int DfuFlash_Write(uint32_t address, const uint8_t *data, uint32_t length)
{
  uint32_t offset = 0U;

  if ((data == NULL) || (range_ok(address, length) == 0))
  {
    return 0;
  }
  HAL_FLASH_Unlock();
  clear_flash_flags();
  while ((((address + offset) & 3U) == 0U) && ((offset + 4U) <= length))
  {
    uint32_t word = 0U;
    memcpy(&word, &data[offset], sizeof word);
    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, address + offset, word) != HAL_OK)
    {
      HAL_FLASH_Lock();
      return 0;
    }
    offset += 4U;
  }
  while (offset < length)
  {
    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE, address + offset, data[offset]) != HAL_OK)
    {
      HAL_FLASH_Lock();
      return 0;
    }
    offset++;
  }
  HAL_FLASH_Lock();
  return 1;
}

int DfuFlash_Read(uint32_t address, uint8_t *dest, uint32_t length)
{
  if ((dest == NULL) || (range_ok(address, length) == 0))
  {
    return 0;
  }
  memcpy(dest, (const void *)address, length);
  return 1;
}
