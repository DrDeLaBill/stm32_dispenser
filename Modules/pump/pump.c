/* Copyright © 2024 Georgy E. All rights reserved. */

#include "pump.h"

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "glog.h"
#include "soul.h"
#include "main.h"
#include "clock.h"
#include "level.h"
#include "gutils.h"
#include "fsm_gc.h"
#include "modbus.h"
#include "gsystem.h"
#include "settings.h"


#define PUMP_MIN_TIME_MS     ((uint32_t)5000)
#define PUMP_WORK_PERIOD     ((uint32_t)300000) //((uint32_t)900000)

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
static bool     _pump_active();
static bool     _pump_ready();


extern settings_t settings;

static const char* TAG = "PUMP";

static bool     settings_updated = false;
static bool     was_enabled      = false;
static uint32_t need_time_ms     = 0;
static gtimer_t timer            = {0};
static gtimer_t wait_timer       = {0};
static gtimer_t indication_timer = {0};


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

FSM_GC_CREATE_ACTION(reset_a,         _reset_a)
FSM_GC_CREATE_ACTION(start_a,         _start_a)
FSM_GC_CREATE_ACTION(down_a,          _down_a)
FSM_GC_CREATE_ACTION(wait_a,          _wait_a)
FSM_GC_CREATE_ACTION(switch_wait_a,   _switch_wait_a)
FSM_GC_CREATE_ACTION(save_and_down_a, _save_and_down_a)
FSM_GC_CREATE_ACTION(error_a,         _error_a)
FSM_GC_CREATE_ACTION(save_and_work_a, _save_and_work_a)
FSM_GC_CREATE_ACTION(save_a,          _save_a)

FSM_GC_CREATE_TABLE(
	pump_fsm_table,
	{&init_s,       &success_e,    &start_s,      &reset_a},

	{&start_s,      &count_work_e, &count_work_s, &start_a},
	{&start_s,      &count_down_e, &count_down_s, &down_a},
	{&start_s,      &count_wait_e, &count_wait_s, &wait_a},

	{&count_work_s, &count_wait_e, &count_wait_s, &switch_wait_a},
	{&count_work_s, &count_down_e, &count_down_s, &save_and_down_a},
	{&count_work_s, &error_e,      &error_s,      &error_a},

	{&count_down_s, &count_wait_e, &count_wait_s, &switch_wait_a},
	{&count_down_s, &count_work_e, &count_work_s, &save_and_work_a},
	{&count_down_s, &error_e,      &error_s,      &error_a},

	{&count_wait_s, &success_e,    &start_s,      &save_a},
	{&count_wait_s, &error_e,      &error_s,      &error_a},

	{&error_s,      &success_e,    &init_s,       &reset_a},
)


void pump_init()
{
	gtimer_start(&timer, 5 * SECOND_MS);
	fsm_gc_init(&pump_fsm, pump_fsm_table, __arr_len(pump_fsm_table));
}

void pump_process()
{
	fsm_gc_process(&pump_fsm);
    _pump_indication_proccess();
}

void pump_show_status()
{
    printTagLog(TAG, "PUMP INFO:");

	int32_t  liquid_val      = get_level();
	uint32_t liquid_adc      = get_level_adc();
	uint16_t pressure_1      = get_press();
#if PUMP_BEDUG
    uint32_t used_day_liquid = settings.pump_work_day_sec * settings.pump_speed / SECOND_MS;
    if (settings.pump_target_ml == 0) {
		printPretty("- no pump_target_ml\n");
	}
    if (settings.pump_speed == 0) {
    	printPretty("- no pump_speed\n");
	}
    if (is_tank_empty()) {
    	printPretty("- tank is empty");
	}
    if (need_time_ms < PUMP_MIN_TIME_MS) {
    	printPretty("- bad work time (%lu ms)\n", need_time_ms);
	}
    if (settings.pump_target_ml <= used_day_liquid) {
    	printPretty("- pump_target_ml overflow\n");
	}
#endif
    uint32_t time_period = 0;
    if (fsm_gc_is_state(&pump_fsm, &count_work_s)) {
        time_period = timer.start + timer.delay > getMillis() ?
			timer.start + timer.delay - getMillis() : 0;
        printPretty("- work [%010lu] => [%010lu] (ms)\n", timer.start, timer.start + timer.delay);
    }
    if (fsm_gc_is_state(&pump_fsm, &count_wait_s)) {
    	time_period = wait_timer.start + wait_timer.delay > getMillis() ?
			wait_timer.start + wait_timer.delay - getMillis() : 0;
        printPretty("- wait [%010lu] => [%010lu] (ms)\n", wait_timer.start, wait_timer.start + wait_timer.delay);
    }
    if (fsm_gc_is_state(&pump_fsm, &count_down_s)) {
        time_period = timer.start + timer.delay - getMillis();
        printPretty("- downtime [%010lu] => [%010lu] (ms)\n", timer.start, timer.start + timer.delay);
    }
    if (time_period) {
    	printPretty("- period: %lu min %lu sec\n", time_period / SECONDS_PER_MINUTE / SECOND_MS, (time_period / SECOND_MS) % SECONDS_PER_MINUTE);
    }
    printPretty("- pressure: %u.%02u MPa\n", pressure_1 / 100, pressure_1 % 100);

    if (get_level() == LEVEL_ERROR) {
    	printPretty("- bad liquid level (ADC=%lu)\n", liquid_adc);
	} else {
		printPretty("- liquid: %ld l (ADC=%lu)\n", liquid_val, liquid_adc);
    }
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
    uint32_t max_pump_ml_to_end_of_day = (time_left * settings.pump_speed) / (MINUTES_PER_HOUR * SECONDS_PER_MINUTE);
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

bool _pump_active()
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

bool _pump_ready()
{
	return _pump_active() && settings.pump_enabled && !has_errors();
}

void _init_s(void)
{
	if (is_system_ready() && !gtimer_wait(&timer)) {
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
	} else if (_pump_ready()) {
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

	if (gtimer_wait(&timer)) {
		return;
	}

	fsm_gc_push_event(&pump_fsm, &count_wait_e);
}

void _count_down_s(void)
{
	if (_pump_ready()) {
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

	if (gtimer_wait(&timer)) {
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

	if (gtimer_wait(&wait_timer)) {
		return;
	}

	fsm_gc_push_event(&pump_fsm, &success_e);
}

void _error_s(void)
{
	if (_pump_active() && is_system_ready()) {
		fsm_gc_push_event(&pump_fsm, &success_e);
	}
}

void _reset_a(void)
{
	fsm_gc_clear(&pump_fsm);
	need_time_ms = 0;
}

void _start_a(void)
{
	was_enabled = settings.pump_enabled;

	gtimer_start(&timer, need_time_ms);
	gtimer_start(&wait_timer, PUMP_WORK_PERIOD);

	HAL_GPIO_WritePin(MOT_FET_GPIO_Port, MOT_FET_Pin, GPIO_PIN_SET);

	printTagLog(TAG, "set %lu ms work", timer.delay);

	pump_show_status();
}

void _down_a(void)
{
	was_enabled = settings.pump_enabled;

	gtimer_start(&timer, need_time_ms);
	gtimer_start(&wait_timer, PUMP_WORK_PERIOD);

	HAL_GPIO_WritePin(MOT_FET_GPIO_Port, MOT_FET_Pin, GPIO_PIN_RESET);

	printTagLog(TAG, "set %lu ms wait", timer.delay);

	pump_show_status();
}

void _wait_a(void)
{
	was_enabled = settings.pump_enabled;

	gtimer_reset(&timer);
	gtimer_start(&wait_timer, PUMP_WORK_PERIOD);

	uint32_t wait_ms = wait_timer.start + wait_timer.delay;
	uint32_t wait_time_ms =  wait_ms - getMillis();
	HAL_GPIO_WritePin(MOT_FET_GPIO_Port, MOT_FET_Pin, GPIO_PIN_RESET);

	printTagLog(TAG, "PUMP OFF - WAIT (%lu ms)", wait_time_ms);
	pump_show_status();
}

void _switch_wait_a(void)
{
	was_enabled = settings.pump_enabled;

	uint32_t wait_ms = wait_timer.start + wait_timer.delay;
	uint32_t wait_time_ms = 0;
	if (getMillis() < wait_ms) {
		wait_time_ms = wait_ms - getMillis();
	}
	if (wait_time_ms > PUMP_MIN_TIME_MS) {
		HAL_GPIO_WritePin(MOT_FET_GPIO_Port, MOT_FET_Pin, GPIO_PIN_RESET);

		printTagLog(TAG, "PUMP OFF - SWITCH WAIT (%lu ms)", wait_time_ms);

		pump_show_status();
	}
}

void _save_and_down_a(void)
{
	settings_updated = false;
	was_enabled = settings.pump_enabled;

	_pump_check_log_date();

	uint32_t res_time_ms = timer.delay;
	uint32_t time_sec    = res_time_ms / SECOND_MS;

	settings.pump_work_day_sec += time_sec;
	settings.pump_work_sec     += time_sec;

#if PUMP_BEDUG
	printTagLog(TAG, "work added (%lu s)", time_sec);
#endif

	set_status(NEED_SAVE_SETTINGS);

	if (need_time_ms > res_time_ms) {
		need_time_ms -= res_time_ms;
	} else {
		need_time_ms = 0;
	}
	gtimer_start(&timer, need_time_ms);

	HAL_GPIO_WritePin(MOT_FET_GPIO_Port, MOT_FET_Pin, GPIO_PIN_RESET);

	printTagLog(TAG, "PUMP DOWNTIME SWITCH (%lu ms)", timer.delay);

	pump_show_status();
}

void _save_and_work_a(void)
{
	settings_updated = false;
	was_enabled = settings.pump_enabled;

	_pump_check_log_date();

	uint32_t res_time_ms = timer.delay;
	uint32_t time_sec    = res_time_ms / SECOND_MS;

    settings.pump_downtime_sec += time_sec;

#if PUMP_BEDUG
    printTagLog(TAG, "downtime added (%lu s)", time_sec);
#endif

	set_status(NEED_SAVE_SETTINGS);

	if (need_time_ms > res_time_ms) {
		need_time_ms -= res_time_ms;
	} else {
		need_time_ms = 0;
	}
	gtimer_start(&timer, need_time_ms);

	HAL_GPIO_WritePin(MOT_FET_GPIO_Port, MOT_FET_Pin, GPIO_PIN_SET);

	printTagLog(TAG, "PUMP WORK SWITCH (%lu ms)", timer.delay);

	pump_show_status();
}

void _save_a(void)
{
	settings_updated = false;
	_pump_check_log_date();

	uint32_t res_time_ms = timer.delay;
	uint32_t time_sec    = res_time_ms / SECOND_MS;

	if (!time_sec) {
		return;
	}

	if (was_enabled) {
		settings.pump_work_day_sec += time_sec;
		settings.pump_work_sec     += time_sec;
#if PUMP_BEDUG
		printTagLog(TAG, "work added (%lu s)", time_sec);
#endif
	} else {
		settings.pump_downtime_sec += time_sec;
#if PUMP_BEDUG
		printTagLog(TAG, "downtime added (%lu s)", time_sec);
#endif
	}

	set_status(NEED_SAVE_SETTINGS);
}

void _error_a(void)
{
	_save_a();

	HAL_GPIO_WritePin(MOT_FET_GPIO_Port, MOT_FET_Pin, GPIO_PIN_RESET);

	printTagLog(TAG, "PUMP OFF");
	if (has_errors()) {
		printTagLog(TAG, "System is not ready");
		show_statuses();
		show_errors();
	}
	if (!_pump_active()) {
		printTagLog(TAG, "Pump is not active");
	}

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

	if (gtimer_wait(&indication_timer)) {
		return;
	}

	GPIO_PinState state = HAL_GPIO_ReadPin(RED_LED_GPIO_Port, RED_LED_Pin);

	if (state) {
		gtimer_start(&indication_timer, PUMP_LED_DISABLE_STATE_OFF_TIME);
	} else {
		gtimer_start(&indication_timer, PUMP_LED_DISABLE_STATE_ON_TIME);
	}

	state = (state == GPIO_PIN_SET) ? GPIO_PIN_RESET : GPIO_PIN_SET;
	HAL_GPIO_WritePin(RED_LED_GPIO_Port, RED_LED_Pin, state);
	HAL_GPIO_WritePin(LAMP_FET_GPIO_Port, LAMP_FET_Pin, state);
}

void _pump_indicate_work_state(uint32_t time)
{
	if (gtimer_wait(&indication_timer)) {
		return;
	}

	GPIO_PinState state = HAL_GPIO_ReadPin(GREEN_LED_GPIO_Port, GREEN_LED_Pin);

	gtimer_start(&indication_timer, time);


	HAL_GPIO_WritePin(RED_LED_GPIO_Port, RED_LED_Pin, state);

	state = (state == GPIO_PIN_SET) ? GPIO_PIN_RESET : GPIO_PIN_SET;
	HAL_GPIO_WritePin(GREEN_LED_GPIO_Port, GREEN_LED_Pin, state);
	HAL_GPIO_WritePin(LAMP_FET_GPIO_Port, LAMP_FET_Pin, state);
}
