/*
 * liquid_sensor.c
 *
 *  Created on: 4 сент. 2022 г.
 *      Author: georg
 */

#include "level.h"

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "glog.h"
#include "main.h"
#include "gutils.h"
#include "gsystem.h"
#include "settings.h"


#define LEVEL_LATENCY (10)


uint16_t _get_liquid_adc_value();
int32_t  _get_liquid_liters(uint32_t adc);
uint32_t _get_cur_liquid_adc();


bool started = false;
unsigned counter = 0;
uint32_t level_adc[10] = {0};
gtimer_t timer = {0};


void level_tick()
{
	if (counter >= __arr_len(level_adc)) {
		started = true;
		counter = 0;
	}
	if (gtimer_wait(&timer)) {
		return;
	}
	gtimer_start(&timer, 100);
	level_adc[counter++] = _get_cur_liquid_adc();

	// TODO: remove start
#ifdef DEBUG
	static gtimer_t tmp_timer = {};
	if (!gtimer_wait(&tmp_timer)) {
		gtimer_start(&tmp_timer, 10 * SECOND_MS);
		printTagLog("TMP", "ADC1: %d, ADC2: %u", get_system_adc(0), get_system_adc(1));
	}
#endif
	// TODO: remove end
}

int32_t get_level()
{
	int32_t sum = 0;
	if (!started) {
		for (unsigned i = 0; i < counter; i++) {
			int32_t level = _get_liquid_liters(level_adc[i]);
			if (level == LEVEL_ERROR) {
				return LEVEL_ERROR;
			}
			sum += level;
		}
	} else {
		for (unsigned i = 0; i < __arr_len(level_adc); i++) {
			int32_t level = _get_liquid_liters(level_adc[i]);
			if (level == LEVEL_ERROR) {
				return LEVEL_ERROR;
			}
			sum += level;
		}
	}
	return sum / (int32_t)__arr_len(level_adc);
}

uint32_t get_level_adc()
{
	if (!started) {
		return (uint32_t)settings.tank_ADC_min;
	}
	uint32_t sum = 0;
	for (unsigned i = 0; i < __arr_len(level_adc); i++) {
		sum += level_adc[i];
	}
	return sum / __arr_len(level_adc);
}

bool is_tank_empty()
{
	return _get_cur_liquid_adc() > settings.tank_ADC_min + LEVEL_LATENCY;
}

uint32_t _get_cur_liquid_adc()
{
	return get_system_adc(0);
}

int32_t _get_liquid_liters(uint32_t adc)
{
	static gtimer_t print_timer = { 0 };

	bool print_flag = false;
	if (!gtimer_wait(&print_timer)) {
		gtimer_start(&print_timer, 3 * SECOND_MS);
		print_flag = true;
	}

	if (adc >= STM_ADC_MAX) {
		if (print_flag) {
			printPretty(
				"- error tank: ADC{%lu} > STM_ADC_MAX{%lu}\n",
				adc,
				STM_ADC_MAX
			);
		}
		return LEVEL_ERROR;
	}

	if (adc > settings.tank_ADC_min + LEVEL_LATENCY ||
		adc + LEVEL_LATENCY < settings.tank_ADC_max
	) {
		if (print_flag) {
			printPretty(
				"- error tank: ADC{%lu} not in tange ADC_min{%lu} -> ADC_max{%lu}\n",
				adc,
				settings.tank_ADC_min,
				settings.tank_ADC_max
			);
		}
		return LEVEL_ERROR;
	}

	if (settings.tank_ADC_min <= settings.tank_ADC_max) {
		if (print_flag) {
			printPretty(
				"- error tank: bad range - ADC_min{%lu} less or equal ADC_max{%lu}\n",
				settings.tank_ADC_min,
				settings.tank_ADC_max
			);
		}
		return LEVEL_ERROR;
	}

	uint32_t adc_range = __abs_dif(settings.tank_ADC_min, settings.tank_ADC_max);
	uint32_t ltr_range = __abs_dif(settings.tank_ltr_max, settings.tank_ltr_min);
	if (adc_range == 0) {
		if (print_flag) {
			printPretty(
				"- error tank: bad range - liters_range=%lu, ADC_range=%lu\n",
				ltr_range,
				adc_range
			);
		}
		return LEVEL_ERROR;
	}

	uint32_t end = adc_range;
	if (adc + LEVEL_LATENCY > settings.tank_ADC_min) {
		return (int32_t)settings.tank_ltr_min;
	}
	if (adc < settings.tank_ADC_max + LEVEL_LATENCY) {
		return (int32_t)settings.tank_ltr_max;
	}
	if (end == 0) {
		if (print_flag) {
			printPretty(
				"- error tank: ADC{%lu}, ADC_min{%lu} -> ADC_max{%lu}\n",
				adc,
				settings.tank_ADC_min,
				settings.tank_ADC_max
			);
		}
		return LEVEL_ERROR;
	}
	uint32_t value = 0;
	if (adc >= settings.tank_ADC_max) {
		value = adc - settings.tank_ADC_max;
	}
	value = end - value;

	int32_t ltr_res = (int32_t)(settings.tank_ltr_min + ((value * ltr_range) / end));
	if (ltr_res <= 0) {
		if (print_flag) {
			printPretty(
				"- error tank: ZERO value (result=%ld l, value=%ld l, range=%ld l, adc_end=%lu)\n",
				ltr_res,
				value,
				ltr_range,
				end
			);
		}
		return LEVEL_ERROR;
	}

	return ltr_res;
}
