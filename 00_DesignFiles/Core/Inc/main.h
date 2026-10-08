/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define ENC_BTN_Pin GPIO_PIN_1
#define ENC_BTN_GPIO_Port GPIOC
#define ENC_B_Pin GPIO_PIN_2
#define ENC_B_GPIO_Port GPIOC
#define ENC_A_Pin GPIO_PIN_3
#define ENC_A_GPIO_Port GPIOC
#define USART2_EXP_TX_Pin GPIO_PIN_2
#define USART2_EXP_TX_GPIO_Port GPIOA
#define USART2_EXP_RX_Pin GPIO_PIN_3
#define USART2_EXP_RX_GPIO_Port GPIOA
#define C1_Pin GPIO_PIN_9
#define C1_GPIO_Port GPIOE
#define C2_Pin GPIO_PIN_10
#define C2_GPIO_Port GPIOE
#define C19_Pin GPIO_PIN_11
#define C19_GPIO_Port GPIOE
#define C18_Pin GPIO_PIN_12
#define C18_GPIO_Port GPIOE
#define C17_Pin GPIO_PIN_13
#define C17_GPIO_Port GPIOE
#define C16_Pin GPIO_PIN_14
#define C16_GPIO_Port GPIOE
#define C15_Pin GPIO_PIN_15
#define C15_GPIO_Port GPIOE
#define C14_Pin GPIO_PIN_10
#define C14_GPIO_Port GPIOB
#define C13_Pin GPIO_PIN_11
#define C13_GPIO_Port GPIOB
#define C12_Pin GPIO_PIN_12
#define C12_GPIO_Port GPIOB
#define C11_Pin GPIO_PIN_13
#define C11_GPIO_Port GPIOB
#define C10_Pin GPIO_PIN_14
#define C10_GPIO_Port GPIOB
#define C9_Pin GPIO_PIN_15
#define C9_GPIO_Port GPIOB
#define C8_Pin GPIO_PIN_8
#define C8_GPIO_Port GPIOD
#define C7_Pin GPIO_PIN_9
#define C7_GPIO_Port GPIOD
#define C6_Pin GPIO_PIN_10
#define C6_GPIO_Port GPIOD
#define C5_Pin GPIO_PIN_11
#define C5_GPIO_Port GPIOD
#define C4_Pin GPIO_PIN_12
#define C4_GPIO_Port GPIOD
#define C3_Pin GPIO_PIN_13
#define C3_GPIO_Port GPIOD
#define R6_Pin GPIO_PIN_14
#define R6_GPIO_Port GPIOD
#define R5_Pin GPIO_PIN_15
#define R5_GPIO_Port GPIOD
#define R4_Pin GPIO_PIN_6
#define R4_GPIO_Port GPIOC
#define R3_Pin GPIO_PIN_7
#define R3_GPIO_Port GPIOC
#define R2_Pin GPIO_PIN_8
#define R2_GPIO_Port GPIOC
#define R1_Pin GPIO_PIN_9
#define R1_GPIO_Port GPIOC
#define USART1_DBG_TX_Pin GPIO_PIN_9
#define USART1_DBG_TX_GPIO_Port GPIOA
#define USART1_DBG_RX_Pin GPIO_PIN_10
#define USART1_DBG_RX_GPIO_Port GPIOA
#define BTN1_Pin GPIO_PIN_0
#define BTN1_GPIO_Port GPIOD

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
