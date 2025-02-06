/* Copyright © 2023 Georgy E. All rights reserved. */

#include "tempr.h"

#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include "main.h"
#include "gutils.h"
#include "gsystem.h"


#define TEMPR_MPA_x100_MAX  ((uint16_t)1600)
#define TEMPR_MPA_x100_MIN  ((uint16_t)0)
#define TEMPR_ADC_VAL_MIN   ((uint16_t)780)
#define TEMPR_ADC_VAL_MAX   ((uint16_t)3916)
#define TEMPR_ADC_CHANNELL  ((uint32_t)5)
#define TEMPR_WAIT_TIME_MS  ((uint16_t)100)
#define TEMPR_MEASURE_COUNT (30)


typedef struct _measure_t {
	bool     measure_ready;
	uint16_t value;
	uint8_t  measure_values_idx;
	uint16_t measure_values[TEMPR_MEASURE_COUNT];
	gtimer_t wait_timer;
} measure_t;


const char* TEMPR_TAG = "TMPR";

static measure_t measure = {
	.measure_ready      = false,
	.value              = 0,
	.measure_values_idx = 0,
	.measure_values     = {0},
	.wait_timer         = {0}
};


uint16_t _tempr_get_adc_value();


void tempr_tick()
{
	if (gtimer_wait(&measure.wait_timer)) {
		return;
	}

	gtimer_start(&measure.wait_timer, TEMPR_WAIT_TIME_MS);

	uint8_t measure_values_len = sizeof(measure.measure_values) / sizeof(*measure.measure_values);

	if (measure.measure_values_idx < measure_values_len) {
		measure.measure_values[measure.measure_values_idx++] = _tempr_get_adc_value();
		return;
	}

	measure.measure_ready = true;
	measure.measure_values_idx = 0;

	uint32_t measure_sum = 0;
	for (uint8_t i = 0; i < measure_values_len; i++) {
		measure_sum += measure.measure_values[i];
	}

	uint16_t adc_value = (uint16_t)(measure_sum / measure_values_len);

	if (adc_value < TEMPR_ADC_VAL_MIN) {
		measure.value = 0;
		return;
	}

	measure.value = adc_value; // TODO: (uint16_t)util_convert_range(adc_value, TEMPR_ADC_VAL_MIN, TEMPR_ADC_VAL_MAX, TEMPR_MPA_x100_MIN, TEMPR_MPA_x100_MAX);
}

uint16_t get_tempr()
{
	if (!measure.measure_ready) {
		return 0;
	}
	return measure.value;
}

uint16_t _tempr_get_adc_value()
{
	return get_system_adc(1);
}

