/* Copyright © 2024 Georgy E. All rights reserved. */

#ifndef _SOUL_H_
#define _SOUL_H_

#ifdef __cplusplus
extern "C" {
#endif


#include <stdint.h>
#include <stdbool.h>

#include "gutils.h"
#include "hal_defs.h"


typedef enum _SOUK_STATUS {
	/* Device statuses start */
	STATUSES_START = 0,

	SYSTEM_HARDWARE_READY,
	SYSTEM_SOFTWARE_READY,
	SYSTEM_SAFETY_MODE,
	SYS_TICK_FAULT,
	MEMORY_INITIALIZED,
	MEMORY_READ_FAULT,
	MEMORY_WRITE_FAULT,
	NEED_MEASURE,
	NEED_STANDBY,
	SETTINGS_INITIALIZED,
	NEED_LOAD_SETTINGS,
	NEED_SAVE_SETTINGS,
	MODBUS_FAULT,
	PUMP_FAULT,
	RTC_FAULT,
	CAN_FAULT,

	DS1307_READY,
	HAS_NEW_RECORD,
	NEW_RECORD_WAS_NOT_SAVED,
	RESERVED_STATUS_04,
	RESERVED_STATUS_05,
	RESERVED_STATUS_06,
	RESERVED_STATUS_07,
	RESERVED_STATUS_08,
	RESERVED_STATUS_09,
	RESERVED_STATUS_10,
	RESERVED_STATUS_11,
	RESERVED_STATUS_12,
	RESERVED_STATUS_13,
	RESERVED_STATUS_14,
	RESERVED_STATUS_15,

	/* Device statuses end */
	STATUSES_END,

	/* Device errors start */
	ERRORS_START,

	RESERVED_ERROR_01,
	RESERVED_ERROR_02,
	RESERVED_ERROR_03,
	RESERVED_ERROR_04,
	RESERVED_ERROR_05,
	RESERVED_ERROR_06,
	RESERVED_ERROR_07,
	RESERVED_ERROR_08,
	RESERVED_ERROR_09,
	RESERVED_ERROR_10,
	RESERVED_ERROR_11,
	RESERVED_ERROR_12,
	RESERVED_ERROR_13,
	RESERVED_ERROR_14,
	RESERVED_ERROR_15,

	MCU_ERROR,
	SYS_TICK_ERROR,
	RTC_ERROR,
	POWER_ERROR,
	EXPECTED_MEMORY_ERROR,
	MEMORY_ERROR,
	STACK_ERROR,
	RAM_ERROR,
	SD_CARD_ERROR,
	USB_ERROR,
	SETTINGS_LOAD_ERROR,
	APP_MODE_ERROR,
	PUMP_ERROR,
	VALVE_ERROR,
	FATFS_ERROR,
	LOAD_ERROR,
	I2C_ERROR,

	NON_MASKABLE_INTERRUPT,
	HARD_FAULT,
	MEM_MANAGE,
	BUS_FAULT,
	USAGE_FAULT,

	ASSERT_ERROR,
	ERROR_HANDLER_CALLED,
	INTERNAL_ERROR,

	/* Device errors end */
	ERRORS_END,

	/* Paste device errors or statuses to the top */
	SOUL_STATUSES_END
} SOUL_STATUS;


typedef struct _soul_t {
#if defined(DEBUG) || defined(GBEDUG_FORCE)
	bool has_new_error_data;
#endif
	SOUL_STATUS last_err;
	uint8_t statuses[__div_up(SOUL_STATUSES_END - 1, BITS_IN_BYTE)];
} soul_t;


SOUL_STATUS get_last_error();
void set_last_error(SOUL_STATUS error);

bool has_errors();

bool is_error(SOUL_STATUS error);
void set_error(SOUL_STATUS error);
void reset_error(SOUL_STATUS error);
SOUL_STATUS get_first_error();

bool is_status(SOUL_STATUS status);
void set_status(SOUL_STATUS status);
void reset_status(SOUL_STATUS status);

char* get_status_name(SOUL_STATUS status);
#if defined(DEBUG) || defined(GBEDUG_FORCE) // TODO: add FAULTS to errors
bool has_new_error_data();
void show_errors();
#endif


#ifdef __cplusplus
}
#endif


#endif
