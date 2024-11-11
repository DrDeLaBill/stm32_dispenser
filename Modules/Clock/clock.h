/* Copyright © 2023 Georgy E. All rights reserved. */

#ifndef _CLOCK_H_
#define _CLOCK_H_


#ifdef __cplusplus
extern "C" {
#endif


#include <stdint.h>
#include <stdbool.h>

#include "hal_defs.h"


#ifdef DEBUG
#   define CLOCK_BEDUG     (0)
#endif


#define SECONDS_PER_MINUTE (60)
#define MINUTES_PER_HOUR   (60)
#define HOURS_PER_DAY      (24)
#define DAYS_PER_WEEK      (7)
#define DAYS_PER_MONTH_MAX (31)
#define MONTHS_PER_YEAR    (12)
#define DAYS_PER_YEAR      (365)
#define DAYS_PER_LEAP_YEAR (366)
#define LEAP_YEAR_PERIOD   ((uint32_t)4)


typedef struct _clock_date_t {
	uint8_t  WeekDay;
	uint8_t  Month;
	uint8_t  Date;
	uint16_t Year;
} clock_date_t;

typedef struct _clock_time_t {
	uint8_t Hours;
	uint8_t Minutes;
	uint8_t Seconds;
} clock_time_t;


uint16_t get_clock_year();
uint8_t  get_clock_month();
uint8_t  get_clock_date();
uint8_t  get_clock_hour();
uint8_t  get_clock_minute();
uint8_t  get_clock_second();
bool     save_clock_time(const clock_time_t* time);
bool     save_clock_date(const clock_date_t* date);
bool     get_clock_rtc_time(clock_time_t* time);
bool     get_clock_rtc_date(clock_date_t* date);
uint32_t get_clock_datetime_to_seconds(const clock_date_t* date, const clock_time_t* time);
uint32_t get_clock_timestamp();
void     get_clock_seconds_to_datetime(const uint32_t seconds, clock_date_t* date, clock_time_t* time);
char*    get_clock_time_format();
char*    get_clock_time_format_by_sec(uint32_t seconds);
bool     set_clock_ready(bool);
bool     is_clock_ready();
bool     get_clock_ram(const uint8_t idx, uint8_t* data);
bool     set_clock_ram(const uint8_t idx, uint8_t data);
bool     is_same_date(const clock_date_t* date1, const clock_date_t* date2);
bool     is_same_time(const clock_time_t* time1, const clock_time_t* time2);


#ifdef __cplusplus
}
#endif


#endif
