/* Copyright © 2024 Georgy E. All rights reserved. */

#include "pump.h"

#include <clock.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "glog.h"
#include "soul.h"
#include "main.h"
#include "level.h"
#include "gutils.h"
#include "system.h"
#include "fsm_gc.h"
#include "settings.h"
#include "pressure.h"


//#define MIN_PUMP_WORK_TIME ((uint32_t)30000)
#define PUMP_MIN_TIME_MS     ((uint32_t)5000)
#define PUMP_WORK_PERIOD     (300000) // ((uint32_t)900000)

#define PUMP_LED_DISABLE_STATE_OFF_TIME ((uint32_t)6000)
#define PUMP_LED_DISABLE_STATE_ON_TIME  ((uint32_t)300)
#define PUMP_LED_WORK_STATE_PERIOD      ((uint32_t)1000)
#define PUMP_LED_OFF_STATE_PERIOD       ((uint32_t)6000)

static void _pump_indication_proccess();
static void _pump_indicate_disable_state();
static void _pump_indicate_work_state();

static uint32_t _calculate_work_time();
static uint32_t _get_day_sec_left();
static void     _pump_check_log_date();
static bool     _pump_ready();


extern settings_t settings;

static const char* TAG = "PUMP";

static bool             settings_updated = false;
static bool             was_enabled      = false;
static uint32_t         need_time_ms     = 0;
static util_old_timer_t timer            = {0};
static util_old_timer_t wait_timer       = {0};
static util_old_timer_t indication_timer = {0};


static void _init_s(void);
static void _start_s(void);
static void _count_work_s(void);
static void _count_wait_s(void);
static void _count_down_s(void);
static void _error_s(void);

static void reset_a(void);
static void start_a(void);
static void down_a(void);
static void wait_a(void);
static void save_and_down_a(void);
static void save_and_work_a(void);
static void save_a(void);
static void error_a(void);


FSM_GC_CREATE(pump_fsm)

FSM_GC_CREATE_EVENT(success_e,    0)
FSM_GC_CREATE_EVENT(count_work_e, 0)
FSM_GC_CREATE_EVENT(count_wait_e, 0)
FSM_GC_CREATE_EVENT(count_down_e, 0)
FSM_GC_CREATE_EVENT(error_e,      1)

FSM_GC_CREATE_STATE(init_s,       _init_s)
FSM_GC_CREATE_STATE(start_s,      _start_s)
FSM_GC_CREATE_STATE(count_work_s, _count_work_s)
FSM_GC_CREATE_STATE(count_wait_s, _count_wait_s)
FSM_GC_CREATE_STATE(count_down_s, _count_down_s)
FSM_GC_CREATE_STATE(error_s,      _error_s)

FSM_GC_CREATE_TABLE(
	pump_fsm_table,
	{&init_s,       &success_e,    &start_s,      reset_a},

	{&start_s,      &count_work_e, &count_work_s, start_a},
	{&start_s,      &count_down_e, &count_down_s, down_a},
	{&start_s,      &count_wait_e, &count_wait_s, wait_a},

	{&count_work_s, &count_wait_e, &count_wait_s, wait_a},
	{&count_work_s, &count_down_e, &count_down_s, save_and_down_a},
	{&count_work_s, &error_e,      &error_s,      error_a},

	{&count_down_s, &count_wait_e, &count_wait_s, wait_a},
	{&count_down_s, &count_work_e, &count_work_s, save_and_work_a},
	{&count_down_s, &error_e,      &error_s,      error_a},

	{&count_wait_s, &success_e,    &start_s,      save_a},
	{&count_wait_s, &error_e,      &error_s,      error_a},

	{&error_s,      &success_e,    &init_s,       reset_a},
)


void pump_init()
{
	fsm_gc_init(&pump_fsm, pump_fsm_table, __arr_len(pump_fsm_table));
}

void pump_process()
{
	fsm_gc_proccess(&pump_fsm);
    _pump_indication_proccess();
}

void pump_show_status()
{
    gprint("################################################\n");

	int32_t  liquid_val      = get_level();
	uint32_t liquid_adc      = get_level_adc();
	uint16_t pressure_1      = get_press();
    uint32_t used_day_liquid = settings.pump_work_day_sec * settings.pump_speed / SECOND_MS;
#if PUMP_BEDUG
    if (settings.pump_target_ml == 0) {
		printTagLog(TAG, "Unable to calculate work time - no setting day liquid target");
	} else if (settings.pump_speed == 0) {
		printTagLog(TAG, "Unable to calculate work time - no setting pump speed");
	} else if (is_tank_empty()) {
		printTagLog(TAG, "Unable to calculate work time - liquid tank empty");
	} else if (need_time_ms < PUMP_MIN_TIME_MS) {
    	printTagLog(TAG, "Unable to calculate work time - needed work time less than %lu sec; set work time 0 sec", PUMP_MIN_TIME_MS / 1000);
	} else if (settings.pump_target_ml <= used_day_liquid) {
		printTagLog(TAG, "Unable to calculate work time - target liquid amount per day already used");
	}
#else
    if (settings.pump_target_ml == 0 ||
		settings.pump_speed == 0 ||
		is_tank_empty() ||
		pump_state.needed_work_time < MIN_PUMP_WORK_TIME ||
		settings.pump_target_ml <= used_day_liquid
	) {
		printTagLog(TAG, "Unable to calculate work time - please check settings");
	}
#endif

    uint32_t time_period = 0;
    if (!settings.pump_speed || !settings.pump_target_ml || get_level() == LEVEL_ERROR) {
		printTagLog(TAG, "Pump will not start - unexceptable settings or sesnors values");
		printTagLog(TAG, "Please check settings: target liters per day, tank ADC values, tank liters values or enable state");
	} else if (fsm_gc_is_state(&pump_fsm, &count_work_s)) {
        time_period = timer.start + timer.delay > getMillis() ?
			timer.start + timer.delay - getMillis() : 0;
        printTagLog(TAG, "Pump work from %lu ms to %lu ms (internal)", timer.start, timer.start + timer.delay);
    } else if (fsm_gc_is_state(&pump_fsm, &count_wait_s)) {
    	time_period = wait_timer.start + wait_timer.delay > getMillis() ?
			wait_timer.start + wait_timer.delay - getMillis() : 0;
        printTagLog(TAG, "Pump will start at %lu ms (internal)", wait_timer.start + wait_timer.delay);
    } else if (fsm_gc_is_state(&pump_fsm, &count_down_s)) {
    	printTagLog(TAG, "Counting pump downtime period");
        time_period = timer.start + timer.delay - getMillis();
        printTagLog(TAG, "Pump count from %lu ms to %lu ms (internal)", timer.start, timer.start + timer.delay);
    } else {
    	printTagLog(TAG, "Pump current day work time: %lu", settings.pump_work_day_sec);
	}

    if (time_period) {
    	printTagLog(TAG, "Wait %lu min %lu sec", time_period / SECONDS_PER_MINUTE / SECOND_MS, (time_period / SECOND_MS) % SECONDS_PER_MINUTE);
    }

    printTagLog(TAG, "Internal clock: %lu ms", getMillis());
    printTagLog(TAG, "Liquid pressure: %u.%02u MPa", pressure_1 / 100, pressure_1 % 100);

    if (liquid_val < 0) {
    	printTagLog(TAG, "Tank liquid value ERR (ADC=%lu)", liquid_adc);
    } else {
    	printTagLog(TAG, "Tank liquid value: %ld l (ADC=%lu)", liquid_val, liquid_adc);
    }
    gprint("################################################\n");
}

void pump_update_speed(uint32_t speed)
{
	if (speed == settings.pump_speed) {
		return;
	}
    settings.pump_speed = speed;
	settings_updated = true;
}

void pump_update_enable_state(bool enabled)
{
	if (settings.pump_enabled == enabled) {
		return;
	}
	settings.pump_enabled = enabled;
	settings_updated = true;
}

void pump_update_ltrmin(uint32_t ltrmin)
{
	if (ltrmin == settings.tank_ltr_min) {
		return;
	}
	settings.tank_ltr_min = ltrmin;
	settings_updated = true;
}

void pump_update_ltrmax(uint32_t ltrmax)
{
	if (ltrmax == settings.tank_ltr_max) {
		return;
	}
	settings.tank_ltr_max = ltrmax;
	settings_updated = true;
}

void pump_update_target(uint32_t target_ltr)
{
	target_ltr *= MILLILITERS_IN_LITER;

	if (target_ltr == settings.pump_target_ml) {
		return;
	}
	settings.pump_target_ml = target_ltr;
	settings_updated = true;
}

uint32_t _calculate_work_time()
{
    uint32_t work_time_sec = 0;

    uint32_t used_day_liquid = (settings.pump_work_day_sec * settings.pump_speed) / (MINUTES_PER_HOUR * SECONDS_PER_MINUTE);
    if (settings.pump_target_ml <= used_day_liquid) {
    	return work_time_sec;
    }

    uint32_t time_left = _get_day_sec_left();
    uint32_t needed_ml = settings.pump_target_ml - used_day_liquid;
    uint32_t max_pump_ml_to_end_of_day = (settings.pump_speed * time_left) / (MINUTES_PER_HOUR * SECONDS_PER_MINUTE);
    if (needed_ml > max_pump_ml_to_end_of_day) {
    	work_time_sec = PUMP_WORK_PERIOD;
        return work_time_sec;
    }

    uint32_t periods_count = (time_left * SECOND_MS) / PUMP_WORK_PERIOD;
    uint32_t needed_ml_per_period = needed_ml / periods_count;
    work_time_sec = (needed_ml_per_period * (MINUTES_PER_HOUR * SECONDS_PER_MINUTE)) / (settings.pump_speed);
    work_time_sec *= SECOND_MS;
    if (work_time_sec > PUMP_WORK_PERIOD) {
    	work_time_sec = PUMP_WORK_PERIOD;
    }
	if (work_time_sec < PUMP_MIN_TIME_MS) {
		work_time_sec = 0;
	}

	return work_time_sec;
}

uint32_t _get_day_sec_left()
{
    return (SECONDS_PER_MINUTE - get_clock_second()) +
           (MINUTES_PER_HOUR - get_clock_minute()) * (uint32_t)SECONDS_PER_MINUTE +
           (HOURS_PER_DAY - get_clock_hour()) * (uint32_t)SECONDS_PER_MINUTE * MINUTES_PER_HOUR;
}

void _pump_check_log_date()
{
	uint8_t cur_date = get_clock_date();
	if (settings.pump_log_date != cur_date) {
#if PUMP_BEDUG
		printTagLog(TAG, "update pump log: day counter - %u -> %u", settings.pump_log_date, cur_date);
#endif
		settings.pump_work_day_sec = 0;
		settings.pump_log_date = cur_date;

		set_status(NEED_SAVE_SETTINGS);
	}
}

bool _pump_ready()
{
    if (settings.pump_target_ml == 0) {
    	return false;
    }
    if (settings.pump_speed == 0) {
    	return false;
    }
    if (get_level() == LEVEL_ERROR) {
    	return false;
    }
    if (is_tank_empty()) {
    	return false;
    }
    return true;
}

void _init_s(void)
{
	if (is_system_ready()) {
		fsm_gc_push_event(&pump_fsm, &success_e);
#if PUMP_BEDUG
		if (settings.pump_target_ml == 0) {
			printTagLog(TAG, "WWARNING - pump init - no setting milliliters_per_day");
		}
		if (settings.pump_speed == 0) {
			printTagLog(TAG, "WWARNING - pump init - no setting pump_speed");
		}
		if (is_tank_empty()) {
			printTagLog(TAG, "WWARNING - pump init - liquid tank empty");
		}
#endif
	}
}

void _start_s(void)
{
	need_time_ms = _calculate_work_time();

	if (need_time_ms < PUMP_MIN_TIME_MS) {
		need_time_ms = 0;
		fsm_gc_push_event(&pump_fsm, &count_wait_e);
	} else if (settings.pump_enabled && _pump_ready() && !has_errors()) {
		fsm_gc_push_event(&pump_fsm, &count_work_e);
	} else {
		fsm_gc_push_event(&pump_fsm, &count_down_e);
	}
}

void _count_work_s(void)
{
	if (!settings.pump_enabled) {
		fsm_gc_push_event(&pump_fsm, &count_down_e);
	}

	if (!_pump_ready()) {
		fsm_gc_push_event(&pump_fsm, &error_e);
	}

	if (has_errors()) {
		fsm_gc_push_event(&pump_fsm, &error_e);
	}

	if (settings_updated) {
		timer.delay = _calculate_work_time();

#if PUMP_BEDUG
		printTagLog(TAG, "Update pump settings");
#endif
		settings_updated = false;
		pump_show_status();
	}

	if (util_old_timer_wait(&timer)) {
		return;
	}

	fsm_gc_push_event(&pump_fsm, &count_wait_e);
}

void _count_down_s(void)
{
	if (settings.pump_enabled && _pump_ready() && !has_errors()) {
		fsm_gc_push_event(&pump_fsm, &count_work_e);
	}

	if (settings_updated) {
		timer.delay = _calculate_work_time();

#if PUMP_BEDUG
		printTagLog(TAG, "Update pump settings");
#endif
		settings_updated = false;
		pump_show_status();
	}

	if (util_old_timer_wait(&timer)) {
		return;
	}

	fsm_gc_push_event(&pump_fsm, &count_wait_e);
}

void _count_wait_s(void)
{
	if (settings_updated) {
#if PUMP_BEDUG
		printTagLog(TAG, "Update pump settings");
#endif
		settings_updated = false;
		pump_show_status();
	}

	if (util_old_timer_wait(&wait_timer)) {
		return;
	}

	fsm_gc_push_event(&pump_fsm, &success_e);
}

void _error_s(void)
{
	if (_pump_ready() && is_system_ready()) {
		fsm_gc_push_event(&pump_fsm, &success_e);
	}
}

void reset_a(void)
{
	fsm_gc_clear(&pump_fsm);
	need_time_ms = 0;
}

void start_a(void)
{
	was_enabled = settings.pump_enabled;

	util_old_timer_start(&timer, need_time_ms);

	HAL_GPIO_WritePin(MOT_FET_GPIO_Port, MOT_FET_Pin, GPIO_PIN_SET);

	printTagLog(TAG, "PUMP ON (will work %lu ms)", timer.delay);

	pump_show_status();
}

void down_a(void)
{
	was_enabled = settings.pump_enabled;

	util_old_timer_start(&timer, need_time_ms);

	HAL_GPIO_WritePin(MOT_FET_GPIO_Port, MOT_FET_Pin, GPIO_PIN_RESET);

	printTagLog(TAG, "PUMP DOWNTIME (%lu ms)", timer.delay);

	pump_show_status();
}

void wait_a(void)
{
	was_enabled = settings.pump_enabled;

	uint32_t wait_time_ms = 0;
	if (need_time_ms <= PUMP_WORK_PERIOD) {
		wait_time_ms = PUMP_WORK_PERIOD - need_time_ms;
	}

	util_old_timer_start(&wait_timer, wait_time_ms);

	if (wait_time_ms > PUMP_MIN_TIME_MS) {
		HAL_GPIO_WritePin(MOT_FET_GPIO_Port, MOT_FET_Pin, GPIO_PIN_RESET);

		printTagLog(TAG, "PUMP OFF - WAIT (%lu ms)", wait_timer.delay);

		pump_show_status();
	}
}

void save_and_down_a(void)
{
	settings_updated = false;
	was_enabled = settings.pump_enabled;

	_pump_check_log_date();

	if (getMillis() < timer.start) {
		return;
	}

	uint32_t res_time_ms = getMillis() - timer.start;
	uint32_t time_sec    = res_time_ms / SECOND_MS;

	settings.pump_work_day_sec += time_sec;
	settings.pump_work_sec     += time_sec;

#if PUMP_BEDUG
	printTagLog(TAG, "update work log: time added (%lu s)", time_sec);
#endif

	set_status(NEED_SAVE_SETTINGS);

	util_old_timer_start(&timer, need_time_ms - res_time_ms);

	HAL_GPIO_WritePin(MOT_FET_GPIO_Port, MOT_FET_Pin, GPIO_PIN_RESET);

	printTagLog(TAG, "PUMP DOWNTIME SWITCH (%lu ms)", timer.delay);

	pump_show_status();
}

void save_and_work_a(void)
{
	settings_updated = false;
	was_enabled = settings.pump_enabled;

	_pump_check_log_date();

	if (getMillis() < timer.start) {
		return;
	}

	uint32_t res_time_ms = getMillis() - timer.start;
	uint32_t time_sec    = res_time_ms / SECOND_MS;

    settings.pump_downtime_sec += time_sec;

#if PUMP_BEDUG
    printTagLog(TAG, "update downtime log: time added (%lu s)", time_sec);
#endif

	set_status(NEED_SAVE_SETTINGS);

	util_old_timer_start(&timer, need_time_ms - res_time_ms);

	HAL_GPIO_WritePin(MOT_FET_GPIO_Port, MOT_FET_Pin, GPIO_PIN_SET);

	printTagLog(TAG, "PUMP WORK SWITCH (%lu ms)", timer.delay);

	pump_show_status();
}

void save_a(void)
{
	settings_updated = false;
	_pump_check_log_date();

	if (getMillis() < timer.start) {
		return;
	}

	uint32_t res_time_ms = getMillis() - timer.start;
	uint32_t time_sec    = res_time_ms / SECOND_MS;

	if (was_enabled) {
		settings.pump_work_day_sec += time_sec;
		settings.pump_work_sec     += time_sec;
#if PUMP_BEDUG
		printTagLog(TAG, "update work log: time added (%lu s)", time_sec);
#endif
	} else {
		settings.pump_downtime_sec += time_sec;
#if PUMP_BEDUG
		printTagLog(TAG, "update downtime log: time added (%lu s)", time_sec);
#endif
	}

	set_status(NEED_SAVE_SETTINGS);
}

void error_a(void)
{
	save_a();

	HAL_GPIO_WritePin(MOT_FET_GPIO_Port, MOT_FET_Pin, GPIO_PIN_RESET);

	printTagLog(TAG, "PUMP OFF - error");
#if PUMP_BEDUG
	if (has_errors()) {
		printTagLog(TAG, "System is not ready");
		show_statuses();
		show_errors();
	}
	if (!_pump_ready()) {
		printTagLog(TAG, "Pump is not ready");
	}
#endif

	pump_show_status();
}

void _pump_indication_proccess()
{
	if (!settings.pump_enabled) {
		_pump_indicate_disable_state();
		return;
	}

	if (fsm_gc_is_state(&pump_fsm, &count_work_s)) {
		_pump_indicate_work_state(PUMP_LED_WORK_STATE_PERIOD);
	} else {
		_pump_indicate_work_state(PUMP_LED_OFF_STATE_PERIOD);
	}
}

void _pump_indicate_disable_state()
{
	HAL_GPIO_WritePin(GREEN_LED_GPIO_Port, GREEN_LED_Pin, GPIO_PIN_RESET);

	if (util_old_timer_wait(&indication_timer)) {
		return;
	}

	GPIO_PinState state = HAL_GPIO_ReadPin(RED_LED_GPIO_Port, RED_LED_Pin);

	if (state) {
		util_old_timer_start(&indication_timer, PUMP_LED_DISABLE_STATE_OFF_TIME);
	} else {
		util_old_timer_start(&indication_timer, PUMP_LED_DISABLE_STATE_ON_TIME);
	}

	state = (state == GPIO_PIN_SET) ? GPIO_PIN_RESET : GPIO_PIN_SET;
	HAL_GPIO_WritePin(RED_LED_GPIO_Port, RED_LED_Pin, state);
	HAL_GPIO_WritePin(LAMP_FET_GPIO_Port, LAMP_FET_Pin, state);
}

void _pump_indicate_work_state(uint32_t time)
{
	if (util_old_timer_wait(&indication_timer)) {
		return;
	}

	GPIO_PinState state = HAL_GPIO_ReadPin(GREEN_LED_GPIO_Port, GREEN_LED_Pin);

	util_old_timer_start(&indication_timer, time);


	HAL_GPIO_WritePin(RED_LED_GPIO_Port, RED_LED_Pin, state);

	state = (state == GPIO_PIN_SET) ? GPIO_PIN_RESET : GPIO_PIN_SET;
	HAL_GPIO_WritePin(GREEN_LED_GPIO_Port, GREEN_LED_Pin, state);
	HAL_GPIO_WritePin(LAMP_FET_GPIO_Port, LAMP_FET_Pin, state);
}
