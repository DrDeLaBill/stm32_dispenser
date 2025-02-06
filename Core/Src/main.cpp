/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "can.h"
#include "dma.h"
#include "i2c.h"
#include "iwdg.h"
#include "spi.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include <string.h>

#include "out.h"
#include "cmd.h"
#include "log.h"
#include "glog.h"
#include "pump.h"
#include "soul.h"
#include "tempr.h"
#include "level.h"
#include "ds1307.h"
#include "gutils.h"
#include "modbus.h"
#include "gsystem.h"
#include "w25qxx.h"
#include "settings.h"
#include "sim_module.h"

#include "Timer.h"


/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

#if defined(DEBUG)
static const char MAIN_TAG[] = "MAIN";
#endif

static char cmd_input_chr = 0;
static char sim_input_chr = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
void init_it();
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

	system_init();

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */
  if (is_error(SYS_TICK_ERROR)) {
	  system_hsi_config();
  } else {
  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */
  }

#ifndef DEBUG
  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_DMA_Init();
  MX_GPIO_Init();
  MX_ADC1_Init();
  MX_I2C1_Init();
  MX_USART1_UART_Init();
  MX_USART3_UART_Init();
  MX_CAN_Init();
  MX_SPI1_Init();
  MX_USART2_UART_Init();
  MX_IWDG_Init();
  /* USER CODE BEGIN 2 */
#else
	MX_DMA_Init();
	MX_GPIO_Init();
	MX_ADC1_Init();
	MX_I2C1_Init();
	MX_USART1_UART_Init();
	MX_USART3_UART_Init();
	MX_CAN_Init();
	MX_SPI1_Init();
	MX_USART2_UART_Init();
#endif
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
    pump_init();

	log_init();

	system_register(settings_update,  20,  true);
	system_register(tempr_tick,       200, true);
	system_register(sim_process,      20,  true);
	system_register(level_tick,       200, true);
	system_register(pump_process,     20,  true);
	system_register(log_tick,         500, true);
	system_register(cmd_process,      20,  true);
//	system_register(modbus_tick,      10,  true);
	system_register(out_tick,         50,  true);

	init_it();

	set_status((SOUL_STATUS)HAS_NEW_RECORD);
	set_system_timeout(150 * SECOND_MS);
	system_start();

	while (1) {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	}
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSI|RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV6;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }

  /** Enables the Clock Security System
  */
  HAL_RCC_EnableCSS();
}

/* USER CODE BEGIN 4 */

void init_it()
{
	HAL_UART_Receive_IT(&SIM_MODULE_UART, (uint8_t*) &sim_input_chr, sizeof(char));
	HAL_UART_Receive_IT(&CMD_UART, (uint8_t*) &cmd_input_chr, sizeof(char));
}

extern "C" void system_hse_config(void)
{
	SystemClock_Config();
}

void system_ready_check(void)
{
#ifndef DEBUG
	HAL_IWDG_Refresh(&hiwdg);
#endif
}

extern "C" void system_error_loop()
{
	static bool initialized = false;
	static gtimer_t led_timer = {};

#ifndef DEBUG
	HAL_IWDG_Refresh(&hiwdg);
#endif

	if (!initialized) {
		GPIO_InitTypeDef GPIO_InitStruct = {};

		__HAL_RCC_GPIOA_CLK_ENABLE();
		__HAL_RCC_GPIOB_CLK_ENABLE();

		GPIO_InitStruct.Pin = RED_LED_Pin;
		GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
		HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

		GPIO_InitStruct.Pin = LAMP_FET_Pin|GREEN_LED_Pin|MOT_FET_Pin;
		GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
		HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

		HAL_GPIO_WritePin(MOT_FET_GPIO_Port, MOT_FET_Pin, GPIO_PIN_RESET);

		HAL_GPIO_WritePin(RED_LED_GPIO_Port, RED_LED_Pin, GPIO_PIN_SET);
		HAL_GPIO_WritePin(GREEN_LED_GPIO_Port, GREEN_LED_Pin, GPIO_PIN_SET);
		HAL_GPIO_WritePin(LAMP_FET_GPIO_Port, LAMP_FET_Pin, GPIO_PIN_SET);

	}

	if (gtimer_wait(&led_timer)) {
		return;
	}
	gtimer_start(&led_timer, 300);
	HAL_GPIO_TogglePin(LAMP_FET_GPIO_Port, LAMP_FET_Pin);
	HAL_GPIO_TogglePin(GREEN_LED_GPIO_Port, GREEN_LED_Pin);
	HAL_GPIO_TogglePin(RED_LED_GPIO_Port, RED_LED_Pin);
}

#define CASE_STATUS(SOUL_STATUS) case SOUL_STATUS:                \
	                                 snprintf(                    \
									     name,                    \
									     sizeof(name) - 1,        \
									     "%s",                    \
									     __STR_DEF__(SOUL_STATUS) \
									 );                           \
									 break;
char* get_custom_status_name(SOUL_STATUS status)
{
	static char name[35] = { 0 };
	memset(name, 0, sizeof(name));

	switch (status) {
	CASE_STATUS(HAS_NEW_RECORD)
	CASE_STATUS(NEW_RECORD_WAS_NOT_SAVED)
	CASE_STATUS(MODBUS_ERROR)
	default:
		snprintf(name, sizeof(name) - 1, "%s", get_custom_status_name(status));
		break;
	}

	return name;
}
#undef CASE_STATUS

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
	if (huart->Instance == SIM_MODULE_UART.Instance) {
		sim_proccess_input(sim_input_chr);
		HAL_UART_Receive_IT(&SIM_MODULE_UART, (uint8_t*)&sim_input_chr, 1);
	} else if (huart->Instance == CMD_UART.Instance) {
		cmd_input(cmd_input_chr);
		HAL_UART_Receive_IT(&CMD_UART, (uint8_t*)&cmd_input_chr, 1);
	} else if (huart->Instance == RS485_UART.Instance) {
		modbus_input();
	} else {
		Error_Handler();
	}
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
#ifdef DEBUG
    b_assert(__FILE__, __LINE__, "The error handler has been called");
#endif
    SOUL_STATUS err = has_errors() ? (SOUL_STATUS)get_first_error() : ERROR_HANDLER_CALLED;
	system_error_handler(err);
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
#ifdef DEBUG
	b_assert((char*)file, line, "Wrong parameters value");
#endif
	SOUL_STATUS err = has_errors() ? (SOUL_STATUS)get_first_error() : ASSERT_ERROR;
	system_error_handler(err);
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
