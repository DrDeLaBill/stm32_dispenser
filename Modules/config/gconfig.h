/* Copyright © 2024 Georgy E. All rights reserved. */

#ifndef _G_SYSTEM_CONFIG_H_
#define _G_SYSTEM_CONFIG_H_


#ifdef __cplusplus
extern "C" {
#endif


#include "soul.h"
#include "hal_defs.h"


//#define GSYSTEM_RESET_TIMEOUT_MS  (30000)

//#define GSYSTEM_NO_RESTART_W
//#define GSYSTEM_NO_RTC_W
#define GSYSTEM_NO_RTC_CALENDAR_W
//#define GSYSTEM_NO_SYS_TICK_W
//#define GSYSTEM_NO_RAM_W
//#define GSYSTEM_NO_ADC_W
//#define GSYSTEM_NO_I2C_W
//#define GSYSTEM_NO_POWER_W
//#define GSYSTEM_NO_MEMORY_W
//#define GSYSTEM_NO_PLL_CHECK_W

//#define GSYSTEM_NO_I2C

#define GSYSTEM_ADC_VOLTAGE_COUNT (3)

#define GSYSTEM_POCESSES_COUNT    (10)

#define GSYSTEM_FLASH_MODE
//#define GSYSTEM_EEPROM_MODE

#define GSYSTEM_DS1307_CLOCK

#define GSYSTEM_TIMER             (TIM4)

#define GSYSTEM_BEDUG_UART        (huart3)

extern I2C_HandleTypeDef          hi2c1;
#define GSYSTEM_I2C               (hi2c1)
//#define GSYSTEM_EEPROM_I2C        GSYSTEM_I2C
#define GSYSTEM_CLOCK_I2C         (hi2c1)
#define GSYSTEM_CLOCK_I2C_BASE    I2C1

#define GSYSTEM_FLASH_SPI         (hspi1)
#define GSYSTEM_FLASH_CS_PORT     (FLASH_CS_GPIO_Port)
#define GSYSTEM_FLASH_CS_PIN      (FLASH_CS_Pin)

#define GSYSTEM_BUTTONS_COUNT     (0)

//#define GSYSTEM_NO_TAMPER_RESET

typedef enum _CUSTOM_SOUL_STATUSES {
	HAS_NEW_RECORD           = RESERVED_STATUS_01,
	NEW_RECORD_WAS_NOT_SAVED = RESERVED_STATUS_02,
	MODBUS_ERROR             = RESERVED_ERROR_01,
} CUSTOM_SOUL_STATUSES;


#ifdef __cplusplus
}
#endif


#endif
