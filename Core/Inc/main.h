/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
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
#include "stm32f1xx_hal.h"

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

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

int _write(int file, uint8_t *ptr, int len);

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define INPUT4_Pin GPIO_PIN_13
#define INPUT4_GPIO_Port GPIOC
#define INPUT5_Pin GPIO_PIN_14
#define INPUT5_GPIO_Port GPIOC
#define INPUT6_Pin GPIO_PIN_15
#define INPUT6_GPIO_Port GPIOC
#define LEVEL_Pin GPIO_PIN_1
#define LEVEL_GPIO_Port GPIOA
#define PRESS_Pin GPIO_PIN_4
#define PRESS_GPIO_Port GPIOA
#define INPUT3_Pin GPIO_PIN_0
#define INPUT3_GPIO_Port GPIOB
#define FLASH_CS_Pin GPIO_PIN_1
#define FLASH_CS_GPIO_Port GPIOB
#define INPUT1_Pin GPIO_PIN_2
#define INPUT1_GPIO_Port GPIOB
#define OUT_A_Pin GPIO_PIN_12
#define OUT_A_GPIO_Port GPIOB
#define OUT_B_Pin GPIO_PIN_13
#define OUT_B_GPIO_Port GPIOB
#define OUT_C_Pin GPIO_PIN_14
#define OUT_C_GPIO_Port GPIOB
#define OUT_D_Pin GPIO_PIN_15
#define OUT_D_GPIO_Port GPIOB
#define SIM_RST_Pin GPIO_PIN_8
#define SIM_RST_GPIO_Port GPIOA
#define LAMP_FET_Pin GPIO_PIN_11
#define LAMP_FET_GPIO_Port GPIOA
#define MOT_FET_Pin GPIO_PIN_12
#define MOT_FET_GPIO_Port GPIOA
#define RED_LED_Pin GPIO_PIN_15
#define RED_LED_GPIO_Port GPIOA
#define INPUT2_Pin GPIO_PIN_4
#define INPUT2_GPIO_Port GPIOB
#define GREEN_LED_Pin GPIO_PIN_5
#define GREEN_LED_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

// General settings
#define GENERAL_TIMEOUT_MS     ((uint32_t)100)

// BEDUG UART
extern UART_HandleTypeDef      huart3;
#define CMD_UART               (huart3)
#define BEDUG_UART             (huart3)

// SIM module
extern UART_HandleTypeDef      huart1;
#define SIM_MODULE_UART        (huart1)
#define SIM_MODULE_RESET_PORT  (SIM_RST_GPIO_Port)
#define SIM_MODULE_RESET_PIN   (SIM_RST_Pin)

// Clock
extern I2C_HandleTypeDef       hi2c1;
#define CLOCK_I2C              (hi2c1)

// FLASH
extern SPI_HandleTypeDef       hspi1;
#define FLASH_SPI              (hspi1)

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
