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
#include "rtc.h"
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
#include "level.h"
#include "ds1307.h"
#include "gutils.h"
#include "system.h"
#include "w25qxx.h"
#include "pressure.h"
#include "settings.h"
#include "sim_module.h"

#include "StorageDriver.h"


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

#if defined(_DEBUG) || defined(DEBUG) || defined(GBEDUG_FORCE)
const char MAIN_TAG[] = "MAIN";
#endif

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

char cmd_input_chr = 0;
char sim_input_chr = 0;

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

	system_pre_load();

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */
  if (is_error(SYS_TICK_ERROR)) {
	  system_clock_hsi_config();
  } else {
	  set_error(SYS_TICK_ERROR);
  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */
	  reset_error(SYS_TICK_ERROR);
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
  MX_RTC_Init();
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
	MX_RTC_Init();
	MX_CAN_Init();
	MX_SPI1_Init();
	MX_USART2_UART_Init();
#endif

	HAL_Delay(100);

    // Pump
    pump_init();
	// Clock
	DS1307_Init();
	// Sim module
	HAL_UART_Receive_IT(&SIM_MODULE_UART, (uint8_t*) &sim_input_chr, sizeof(char));
	// CMD module
	HAL_UART_Receive_IT(&SIM_MODULE_UART, (uint8_t*) &cmd_input_chr, sizeof(char));

	gprint("\n\n\n");
	printTagLog(MAIN_TAG, "The device is loading");

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
	utl::Timer errTimer(300 * SECOND_MS);

	set_error(STACK_ERROR);
	errTimer.start();
	while (has_errors() || !is_status(SYSTEM_HARDWARE_READY)) {
		system_tick();

		if (!errTimer.wait()) {
			system_error_handler((SOUL_STATUS)get_first_error());
		}
	}

	sim_begin();

	log_init();

	system_post_load();

	HAL_Delay(100);

	printTagLog(MAIN_TAG, "The device has been loaded\n");


	// TODO: remove start
#ifdef DEBUG
	util_old_timer_t tmp_timer = {};
#endif
	// TODO: remove end

	set_status(HAS_NEW_RECORD);
	errTimer.start();
	util_old_timer_start(&tmp_timer, 10000);
	while (1)
	{
		// TODO: remove start
#ifdef DEBUG
		static unsigned counter = 0;
		if (!util_old_timer_wait(&tmp_timer)) {
			util_old_timer_start(&tmp_timer, 1000);
			HAL_UART_Transmit(&huart2, (uint8_t*)&counter, sizeof(counter), 100);
			counter++;
		}
#endif
		// TODO: remove end

		system_tick();

		settings_update();

		if (!errTimer.wait()) {
			system_error_handler((SOUL_STATUS)get_first_error());
		}

        // Pump
        pump_process();

        out_tick();

		// Pressure update
		pressure_process();

        // Sim module
        sim_process();

        // Level update
        level_tick();

        // Record & settings synchronize process
        log_tick();

        // CMD process
        cmd_process();

#ifndef DEBUG
		HAL_IWDG_Refresh(&hiwdg);
#endif

		if (!is_system_ready()) {
			continue;
		}
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
		errTimer.start();
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
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_RTC|RCC_PERIPHCLK_ADC;
  PeriphClkInit.RTCClockSelection = RCC_RTCCLKSOURCE_LSI;
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

void system_error_loop()
{
	static bool initialized = false;
	static util_old_timer_t led_timer = {};

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

	if (util_old_timer_wait(&led_timer)) {
		return;
	}
	util_old_timer_start(&led_timer, 300);
	HAL_GPIO_TogglePin(LAMP_FET_GPIO_Port, LAMP_FET_Pin);
	HAL_GPIO_TogglePin(GREEN_LED_GPIO_Port, GREEN_LED_Pin);
	HAL_GPIO_TogglePin(RED_LED_GPIO_Port, RED_LED_Pin);
}

void HAL_RCC_CSSCallback(void)
{
	system_sys_tick_reanimation();
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
	if (huart->Instance == SIM_MODULE_UART.Instance) {
		sim_proccess_input(sim_input_chr);
		HAL_UART_Receive_IT(&SIM_MODULE_UART, (uint8_t*)&sim_input_chr, 1);
	} else if (huart->Instance == CMD_UART.Instance) {
		cmd_input(cmd_input_chr);
		HAL_UART_Receive_IT(&CMD_UART, (uint8_t*)&cmd_input_chr, 1);
	} else {
		Error_Handler();
	}
}

int _write(int, uint8_t *ptr, int len) {
    HAL_UART_Transmit(&BEDUG_UART, (uint8_t*)ptr, static_cast<uint16_t>(len), GENERAL_TIMEOUT_MS);
#ifdef DEBUG
    for (int DataIdx = 0; DataIdx < len; DataIdx++) {
        ITM_SendChar(*ptr++);
    }
    return len;
#endif
    return 0;
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
