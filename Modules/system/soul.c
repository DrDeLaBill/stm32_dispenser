/* Copyright © 2024 Georgy E. All rights reserved. */

#include "soul.h"

#include <stdint.h>
#include <stdbool.h>

#include "glog.h"
#include "bmacro.h"


#if defined(DEBUG) || defined(GBEDUG_FORCE)
static const char TAG[] = "SOUL";
#endif

static soul_t soul = {
	.has_new_error_data = false,
	.last_err           = 0,
	.statuses           = { 0 }
};


bool _is_status(SOUL_STATUS status);
void _set_status(SOUL_STATUS status);
void _reset_status(SOUL_STATUS status);


SOUL_STATUS get_last_error()
{
	return soul.last_err;
}

void set_last_error(SOUL_STATUS error)
{
	if (ERRORS_START < error && error < ERRORS_END) {
		soul.last_err = error;
	}
}

bool has_errors()
{
	for (unsigned i = ERRORS_START + 1; i < ERRORS_END; i++) {
		if (is_error((SOUL_STATUS)(i))) {
			return true;
		}
	}
	return false;
}

bool is_error(SOUL_STATUS error)
{
	if (error > ERRORS_START && error < ERRORS_END) {
		return _is_status(error);
	}
	return false;
}

void set_error(SOUL_STATUS error)
{
	if (error > ERRORS_START && error < ERRORS_END) {
#if defined(DEBUG) || defined(GBEDUG_FORCE)
		if (!is_error(error)) {
			soul.has_new_error_data = true;
		}
#endif
		_set_status(error);
	}
}

SOUL_STATUS get_first_error()
{
	for (unsigned i = ERRORS_START + 1; i < ERRORS_END; i++) {
		if (is_error((SOUL_STATUS)(i))) {
			return i;
		}
	}
	return 0;
}

void reset_error(SOUL_STATUS error)
{
	if (error > ERRORS_START && error < ERRORS_END) {
#if defined(DEBUG) || defined(GBEDUG_FORCE)
		if (is_error(error)) {
			soul.has_new_error_data = true;
		}
#endif
		_reset_status(error);
	}
}

bool is_status(SOUL_STATUS status)
{
	if (status > STATUSES_START && status < STATUSES_END) {
		return _is_status(status);
	}
	return false;
}

void set_status(SOUL_STATUS status)
{
	if (status > STATUSES_START && status < STATUSES_END) {
		_set_status(status);
	}
}

void reset_status(SOUL_STATUS status)
{
	if (status > STATUSES_START && status < STATUSES_END) {
		_reset_status(status);
	}
}

bool _is_status(SOUL_STATUS status)
{
	uint8_t status_num = (uint8_t)(status) - 1;
	return (bool)(
		(
			soul.statuses[status_num / BITS_IN_BYTE] >>
			(status_num % BITS_IN_BYTE)
		) & 0x01
	);
}

void _set_status(SOUL_STATUS status)
{
	if (status == 0) {
		return;
	}
	uint8_t status_num = (uint8_t)(status) - 1;
	soul.statuses[status_num / BITS_IN_BYTE] |= (0x01 << (status_num % BITS_IN_BYTE));
}

void _reset_status(SOUL_STATUS status)
{
	if (status == 0) {
		return;
	}
	uint8_t status_num = (uint8_t)(status) - 1;
	soul.statuses[status_num / BITS_IN_BYTE] &= (uint8_t)~(0x01 << (status_num % BITS_IN_BYTE));
}

#define CASE_STATUS(SOUL_ERR) case SOUL_ERR: snprintf(name, sizeof(name) - 1, "%s [%u]", __STR_DEF__(SOUL_ERR), SOUL_ERR); break;
char* get_status_name(SOUL_STATUS status)
{
	const char UNKNOWN_STATUS[] = "UNKNOWN_STATUS";

	static char name[35] = { 0 };
	memset(name, 0, sizeof(name));

	switch (status) {
	CASE_STATUS(SYSTEM_HARDWARE_READY)
	CASE_STATUS(SYSTEM_SOFTWARE_READY)
	CASE_STATUS(SYSTEM_SAFETY_MODE)
	CASE_STATUS(SYS_TICK_FAULT)
	CASE_STATUS(MEMORY_INITIALIZED)
	CASE_STATUS(MEMORY_READ_FAULT)
	CASE_STATUS(MEMORY_WRITE_FAULT)
	CASE_STATUS(NEED_MEASURE)
	CASE_STATUS(NEED_STANDBY)
	CASE_STATUS(SETTINGS_INITIALIZED)
	CASE_STATUS(NEED_LOAD_SETTINGS)
	CASE_STATUS(NEED_SAVE_SETTINGS)
	CASE_STATUS(MODBUS_FAULT)
	CASE_STATUS(PUMP_FAULT)
	CASE_STATUS(RTC_FAULT)
	CASE_STATUS(CAN_FAULT)
	CASE_STATUS(MCU_ERROR)
	CASE_STATUS(SYS_TICK_ERROR)
	CASE_STATUS(RTC_ERROR)
	CASE_STATUS(POWER_ERROR)
	CASE_STATUS(EXPECTED_MEMORY_ERROR)
	CASE_STATUS(MEMORY_ERROR)
	CASE_STATUS(STACK_ERROR)
	CASE_STATUS(RAM_ERROR)
	CASE_STATUS(SD_CARD_ERROR)
	CASE_STATUS(USB_ERROR)
	CASE_STATUS(SETTINGS_LOAD_ERROR)
	CASE_STATUS(APP_MODE_ERROR)
	CASE_STATUS(PUMP_ERROR)
	CASE_STATUS(VALVE_ERROR)
	CASE_STATUS(FATFS_ERROR)
	CASE_STATUS(LOAD_ERROR)
	CASE_STATUS(I2C_ERROR)
	CASE_STATUS(NON_MASKABLE_INTERRUPT)
	CASE_STATUS(HARD_FAULT)
	CASE_STATUS(MEM_MANAGE)
	CASE_STATUS(BUS_FAULT)
	CASE_STATUS(USAGE_FAULT)
	CASE_STATUS(ASSERT_ERROR)
	CASE_STATUS(ERROR_HANDLER_CALLED)
	CASE_STATUS(INTERNAL_ERROR)
	default:
		snprintf(name, sizeof(name) - 1, "%s [%u]", UNKNOWN_STATUS, status);
		break;
	}

	return name;
}

#if defined(DEBUG) || defined(GBEDUG_FORCE)

bool has_new_error_data()
{
	return soul.has_new_error_data;
}

void show_statuses()
{
	printTagLog(TAG, "Current device statuses:");

	unsigned cnt = 0;
	for (SOUL_STATUS i = STATUSES_START + 1; i < STATUSES_END; i++) {
		if (!is_status(i)) {
			continue;
		}
		cnt++;
		printPretty("%s\n", get_status_name(i));
	}
	if (!cnt) {
		printPretty("NO_STATUSES\n");
	}
}

void show_errors()
{
	soul.has_new_error_data = false;
	printTagLog(TAG, "Current device errors:");

	unsigned cnt = 0;
	for (SOUL_STATUS i = ERRORS_START + 1; i < ERRORS_END; i++) {
		if (!is_error(i)) {
			continue;
		}
		cnt++;
		printPretty("%s\n", get_status_name(i));
	}
	if (!cnt) {
		printPretty("NO_ERROR\n");
	}
}

#endif
