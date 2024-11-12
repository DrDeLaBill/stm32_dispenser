/* Copyright © 2024 Georgy E. All rights reserved. */

#include <cmath>
#include <cstdint>

#include "soul.h"
#include "glog.h"
#include "clock.h"
#include "w25qxx.h"
#include "system.h"
#include "hal_defs.h"

#include "StorageAT.h"
#include "StorageDriver.h"


#define WATCHDOG_BEDUG
#define SYSTEM_FLASH_MODE


static const char TAG[] = "SYS";

StorageDriver storageDriver;
StorageAT storage(
#if defined(SYSTEM_EEPROM_MDE)
	EEPROM_PAGES_COUNT
#elif defined(SYSTEM_FLASH_MODE)
	0,
#else
#   error "Memory mode is not selected"
	0,
#endif
	&storageDriver,
#if defined(SYSTEM_EEPROM_MDE)
	EEPROM_PAGE_SIZE
#elif defined(SYSTEM_FLASH_MODE)
	FLASH_W25_SECTOR_SIZE
#else
	0
#endif
);


extern "C" void sys_clock_watchdog_check()
{
	if (!is_error(SYS_TICK_ERROR) && !is_status(SYS_TICK_FAULT)) {
		return;
	}

	system_sys_tick_reanimation();
}

extern "C" void ram_watchdog_check()
{
	static const unsigned STACK_PERCENT_MIN = 5;
	static unsigned lastFree = 0;

	extern unsigned _ebss;
	unsigned *start, *end;
	__asm__ volatile ("mov %[end], sp" : [end] "=r" (end) : : );
	start = &_ebss;

	unsigned heap_end = 0;
	unsigned stack_end = 0;
	unsigned last_counter = 0;
	unsigned cur_counter = 0;
	for (;start < end; start++) {
		if ((*start) == SYSTEM_CANARY_WORD) {
			cur_counter++;
		}
		if (cur_counter && (*start) != SYSTEM_CANARY_WORD) {
			if (last_counter < cur_counter) {
				last_counter = cur_counter;
				heap_end     = (unsigned)start - cur_counter;
				stack_end    = (unsigned)start;
			}

			cur_counter = 0;
		}
	}

	extern unsigned _sdata;
	extern unsigned _estack;
	uint32_t freeRamBytes = last_counter * sizeof(SYSTEM_CANARY_WORD);
	unsigned freePercent = (unsigned)__percent(
		(uint32_t)last_counter,
		(uint32_t)__abs_dif(&_sdata, &_estack)
	);
#ifdef WATCHDOG_BEDUG
	if (freeRamBytes && __abs_dif(lastFree, freeRamBytes)) {
		printTagLog(TAG, "-----ATTENTION! INDIRECT DATA BEGIN:-----");
		printTagLog(TAG, "RAM:              [0x%08X->0x%08X]", (unsigned)&_sdata, (unsigned)&_estack);
		printTagLog(TAG, "RAM occupied MAX: %u bytes", (unsigned)(__abs_dif((unsigned)&_sdata, (unsigned)&_estack) - freeRamBytes));
		printTagLog(TAG, "RAM free  MIN:    %u bytes (%u%%) [0x%08X->0x%08X]", (unsigned)freeRamBytes, freePercent, (unsigned)(stack_end - freeRamBytes), (unsigned)stack_end);
		printTagLog(TAG, "------ATTENTION! INDIRECT DATA END-------");
	}
#endif

	if (freeRamBytes) {
		lastFree = freeRamBytes;
	}

	if (freeRamBytes && lastFree && heap_end < stack_end && freePercent > STACK_PERCENT_MIN) {
		reset_error(STACK_ERROR);
	} else {
#ifdef WATCHDOG_BEDUG
		BEDUG_ASSERT(
			is_error(STACK_ERROR),
			"STACK OVERFLOW IS POSSIBLE or the function STACK_WATCHDOG_FILL_RAM was not used on startup"
		);
#endif
		set_error(STACK_ERROR);
	}
}

extern "C" void rtc_watchdog_check()
{
	static bool tested = false;

	if (is_clock_ready()) {
		set_status(DS1307_READY);
	} else {
		reset_status(DS1307_READY);
	}

	if (!is_status(DS1307_READY)) {
		return;
	}

	if (is_error(RTC_ERROR)) {
		tested = false;
	}

	if (tested) {
		return;
	}

#ifdef DEBUG
	printTagLog(TAG, "RTC testing in progress...");
#endif

	clock_date_t readDate = {0,0,0,0};
	clock_time_t readTime = {0,0,0};

#ifdef WATCHDOG_BEDUG
	printPretty("Get date test: ");
#endif
	if (!get_clock_rtc_date(&readDate)) {
#ifdef WATCHDOG_BEDUG
		gprint("   error\n");
#endif
		set_error(RTC_ERROR);
		return;
	}
#ifdef WATCHDOG_BEDUG
	gprint("   OK\n");
	printPretty("Get time test: ");
#endif
	if (!get_clock_rtc_time(&readTime)) {
#ifdef WATCHDOG_BEDUG
		gprint("   error\n");
#endif
		set_error(RTC_ERROR);
		return;
	}
#ifdef WATCHDOG_BEDUG
	gprint("   OK\n");
	printPretty("Save date test: ");
#endif
	if (!save_clock_date(&readDate)) {
#ifdef WATCHDOG_BEDUG
		gprint("  error\n");
#endif
		set_error(RTC_ERROR);
		return;
	}
#ifdef WATCHDOG_BEDUG
	gprint("  OK\n");
	printPretty("Save time test: ");
#endif
	if (!save_clock_time(&readTime)) {
#ifdef WATCHDOG_BEDUG
		gprint("  error\n");
#endif
		set_error(RTC_ERROR);
		return;
	}
#ifdef WATCHDOG_BEDUG
	gprint("  OK\n");
#endif


	clock_date_t checkDate = {0,0,0,0};
	clock_time_t checkTime = {0,0,0};
#ifdef WATCHDOG_BEDUG
	printPretty("Check date test: ");
#endif
	if (!get_clock_rtc_date(&checkDate)) {
#ifdef WATCHDOG_BEDUG
		gprint(" error\n");
#endif
		set_error(RTC_ERROR);
		return;
	}
	if (memcmp((void*)&readDate, (void*)&checkDate, sizeof(readDate))) {
#ifdef WATCHDOG_BEDUG
		gprint(" error\n");
#endif
		set_error(RTC_ERROR);
		return;
	}
#ifdef WATCHDOG_BEDUG
	gprint(" OK\n");
	printPretty("Check time test: ");
#endif
	if (!get_clock_rtc_time(&checkTime)) {
#ifdef WATCHDOG_BEDUG
		gprint(" error\n");
#endif
		set_error(RTC_ERROR);
		return;
	}
	if (!is_same_time(&readTime, &checkTime)) {
#ifdef WATCHDOG_BEDUG
		gprint(" error\n");
#endif
		set_error(RTC_ERROR);
		return;
	}
#ifdef WATCHDOG_BEDUG
	gprint(" OK\n");
#endif


#ifdef WATCHDOG_BEDUG
	printPretty("Weekday test\n");
#endif
	const clock_date_t dates[] = {
		{RTC_WEEKDAY_SATURDAY,  01, 01, 00},
		{RTC_WEEKDAY_SUNDAY,    01, 02, 00},
		{RTC_WEEKDAY_SATURDAY,  04, 27, 24},
		{RTC_WEEKDAY_SUNDAY,    04, 28, 24},
		{RTC_WEEKDAY_MONDAY,    04, 29, 24},
		{RTC_WEEKDAY_TUESDAY,   04, 30, 24},
		{RTC_WEEKDAY_WEDNESDAY, 05, 01, 24},
		{RTC_WEEKDAY_THURSDAY,  05, 02, 24},
		{RTC_WEEKDAY_FRIDAY,    05, 03, 24},
	};
#if defined(STM32F1)
	const clock_time_t times[] = {
		{00, 00, 00},
		{00, 00, 00},
		{03, 24, 49},
		{04, 14, 24},
		{03, 27, 01},
		{23, 01, 40},
		{03, 01, 40},
		{04, 26, 12},
		{03, 52, 35},
	};
#elif defined(STM32F4)
	const RTC_TimeTypeDef times[] = {
		{00, 00, 00, 0, 0, 0, 0, 0},
		{00, 00, 00, 0, 0, 0, 0, 0},
		{03, 24, 49, 0, 0, 0, 0, 0},
		{04, 14, 24, 0, 0, 0, 0, 0},
		{03, 27, 01, 0, 0, 0, 0, 0},
		{23, 01, 40, 0, 0, 0, 0, 0},
		{03, 01, 40, 0, 0, 0, 0, 0},
		{04, 26, 12, 0, 0, 0, 0, 0},
		{03, 52, 35, 0, 0, 0, 0, 0},
	};
#endif
	const uint64_t seconds[] = {
		0,
		86400,
		767503489,
		767592864,
		767676421,
		767833300,
		767847700,
		767939172,
		768023555,
	};

	for (unsigned i = 0; i < __arr_len(seconds); i++) {
#ifdef WATCHDOG_BEDUG
		printPretty("[%02u]: ", i);
#endif

		clock_date_t tmpDate = {0,0,0,0};
		clock_time_t tmpTime = {0,0,0};
		get_clock_seconds_to_datetime(seconds[i], &tmpDate, &tmpTime);
		if (memcmp((void*)&tmpDate, (void*)&dates[i], sizeof(tmpDate))) {
#ifdef WATCHDOG_BEDUG
			gprint("            error\n");
#endif
			set_error(RTC_ERROR);
			return;
		}
		if (!is_same_time(&tmpTime, &times[i])) {
#ifdef WATCHDOG_BEDUG
			gprint("            error\n");
#endif
			set_error(RTC_ERROR);
			return;
		}

		uint64_t tmpSeconds = get_clock_datetime_to_seconds(&dates[i], &times[i]);
		if (tmpSeconds != seconds[i]) {
#ifdef WATCHDOG_BEDUG
			gprint("            error\n");
#endif
			set_error(RTC_ERROR);
			return;
		}

#ifdef WATCHDOG_BEDUG
		gprint("            OK\n");
#endif
	}

	tested = true;


#ifdef WATCHDOG_BEDUG
	printTagLog(TAG, "RTC testing done");
#endif
}

extern "C" void memory_watchdog_check()
{
	static const uint32_t TIMEOUT_MS = 15000;
	static const unsigned ERRORS_MAX = 5;

	static utl::Timer errorTimer(TIMEOUT_MS);
	static utl::Timer timer(SECOND_MS);
	static uint8_t errors = 0;
	static bool timerStarted = false;

	uint8_t data = 0;
#ifdef EEPROM_MODE
	eeprom_status_t status = EEPROM_OK;
#else
	flash_status_t status = FLASH_OK;
#endif

#ifndef EEPROM_MODE
	if (!is_status(MEMORY_INITIALIZED)) {
		if (flash_w25qxx_init() == FLASH_OK) {
			set_status(MEMORY_INITIALIZED);
			storage.setPagesCount(flash_w25qxx_get_pages_count());
#ifdef WATCHDOG_BEDUG
			printTagLog(TAG, "flash init success (%lu pages)", flash_w25qxx_get_pages_count());
#endif
		} else {
#ifdef WATCHDOG_BEDUG
			printTagLog(TAG, "flash init error");
#endif
		}
		return;
	}
#endif

	if (is_status(MEMORY_READ_FAULT) ||
		is_status(MEMORY_WRITE_FAULT) ||
		is_error(MEMORY_ERROR)
	) {
#ifdef SYSTEM_EEPROM_MODE
		system_reset_i2c_errata();

		uint32_t address = static_cast<uint32_t>(rand()) % eeprom_get_size();

		status = eeprom_read(address, &data, sizeof(data));
		if (status == EEPROM_OK) {
			reset_status(MEMORY_READ_FAULT);
			status = eeprom_write(address, &data, sizeof(data));
		} else {
			errors++;
		}
		if (status == EEPROM_OK) {
			reset_status(MEMORY_WRITE_FAULT);
			timerStarted = false;
			errors = 0;
		} else {
			errors++;
		}
#elif defined(SYSTEM_FLASH_MODE)
		if (is_status(MEMORY_INITIALIZED) && flash_w25qxx_init() != FLASH_OK) {
			reset_status(MEMORY_INITIALIZED);
		}

		uint32_t address = static_cast<uint32_t>(rand()) % (flash_w25qxx_get_pages_count() * FLASH_W25_PAGE_SIZE);

		status = flash_w25qxx_read(address, &data, sizeof(data));
		if (status == FLASH_OK) {
			reset_status(MEMORY_READ_FAULT);
			status = flash_w25qxx_write(address, &data, sizeof(data));
		} else {
			errors++;
		}
		if (status == FLASH_OK) {
			reset_status(MEMORY_WRITE_FAULT);
			timerStarted = false;
			errors = 0;
		} else {
			errors++;
		}
#endif
	}

	(errors > ERRORS_MAX) ? set_error(MEMORY_ERROR) : reset_error(MEMORY_ERROR);

	if (!timerStarted && is_error(MEMORY_ERROR)) {
		timerStarted = true;
		errorTimer.start();
	}

	if (timerStarted && !errorTimer.wait()) {
		system_error_handler(MEMORY_ERROR);
	}
}

extern "C" void power_watchdog_check()
{
	uint32_t voltage = get_system_power();

	if (STM_MIN_VOLTAGEx10 <= voltage && voltage <= STM_MAX_VOLTAGEx10) {
		reset_error(POWER_ERROR);
	} else {
		set_error(POWER_ERROR);
	}
}

extern "C" void restart_watchdog_check()
{
	static bool flagsCleared = false;

	if (flagsCleared) {
		return;
	}

	bool flag = false;
	// IWDG check reboot
	if (__HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST)) {
#ifdef WATCHDOG_BEDUG
		printTagLog(TAG, "IWDG just went off");
#endif
		flag = true;
	}

	// WWDG check reboot
	if (__HAL_RCC_GET_FLAG(RCC_FLAG_WWDGRST)) {
#ifdef WATCHDOG_BEDUG
		printTagLog(TAG, "WWDG just went off");
#endif
		flag = true;
	}

	if (__HAL_RCC_GET_FLAG(RCC_FLAG_SFTRST)) {
#ifdef WATCHDOG_BEDUG
		printTagLog(TAG, "SOFT RESET");
#endif
		flag = true;
	}

	if (flag) {
		__HAL_RCC_CLEAR_RESET_FLAGS();
#ifdef WATCHDOG_BEDUG
		printTagLog(TAG, "DEVICE HAS BEEN REBOOTED");
#endif
		system_reset_i2c_errata();
		HAL_Delay(2500);
	}
}
