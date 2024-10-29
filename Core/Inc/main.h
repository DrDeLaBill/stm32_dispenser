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
#define FLASH_CS_Pin GPIO_PIN_12
#define FLASH_CS_GPIO_Port GPIOB
#define SIM_RST_Pin GPIO_PIN_11
#define SIM_RST_GPIO_Port GPIOA

/* USER CODE BEGIN Private defines */

// General settings
#define GENERAL_TIMEOUT_MS     ((uint32_t)100)

// BEDUG UART
extern UART_HandleTypeDef      huart3;
#define COMMAND_UART           (huart3)
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
extern SPI_HandleTypeDef       hspi2;
#define FLASH_SPI              (hspi2)

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
