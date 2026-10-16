#include "boot.h"

#include "main.h"
#include "stm32f4xx_hal.h"

static int application_is_valid(void)
{
  uint32_t stack = *(volatile uint32_t *)APP_FLASH_BASE;
  uint32_t reset = *(volatile uint32_t *)(APP_FLASH_BASE + 4UL);

  if ((stack < 0x20000000UL) || (stack > 0x20020000UL))
  {
    return 0;
  }
  if ((reset < (APP_FLASH_BASE + 4UL)) || (reset >= APP_FLASH_END))
  {
    return 0;
  }
  if ((reset & 1UL) == 0UL)
  {
    return 0;
  }
  return 1;
}

int Boot_EnterDfu(void)
{
  GPIO_InitTypeDef gpio = {0};

  __HAL_RCC_GPIOD_CLK_ENABLE();
  gpio.Pin = BTN1_Pin;
  gpio.Mode = GPIO_MODE_INPUT;
  gpio.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(BTN1_GPIO_Port, &gpio);

  if (HAL_GPIO_ReadPin(BTN1_GPIO_Port, BTN1_Pin) == GPIO_PIN_RESET)
  {
    HAL_Delay(20);
    if (HAL_GPIO_ReadPin(BTN1_GPIO_Port, BTN1_Pin) == GPIO_PIN_RESET)
    {
      return 1;
    }
  }
  if (application_is_valid() == 0)
  {
    return 1;
  }
  return 0;
}

void Boot_JumpToApplication(void)
{
  uint32_t stack = *(volatile uint32_t *)APP_FLASH_BASE;
  uint32_t reset = *(volatile uint32_t *)(APP_FLASH_BASE + 4UL);
  void (*entry)(void) = (void (*)(void))reset;
  uint32_t irq;

  __disable_irq();
  SysTick->CTRL = 0U;
  SysTick->LOAD = 0U;
  SysTick->VAL = 0U;
  HAL_RCC_DeInit();
  HAL_DeInit();
  for (irq = 0; irq < 8U; irq++)
  {
    NVIC->ICER[irq] = 0xFFFFFFFFU;
    NVIC->ICPR[irq] = 0xFFFFFFFFU;
  }
  SCB->VTOR = APP_FLASH_BASE;
  __DSB();
  __ISB();
  __set_CONTROL(0U);
  __set_MSP(stack);
  __enable_irq();
  entry();
  while (1)
  {
  }
}
