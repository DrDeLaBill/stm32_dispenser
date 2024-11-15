/* Copyright © 2024 Georgy E. All rights reserved. */

#include "cmd.h"

#include <string.h>
#include <stdbool.h>

#include "glog.h"
#include "soul.h"
#include "pump.h"
#include "level.h"
#include "gutils.h"
#include "system.h"
#include "settings.h"


#define STR_CMD_SIZE (20)
#define CMD_DELAY_MS (1000)


static void _clear();

static void _cmd_status();
static void _cmd_saveadcmin();
static void _cmd_saveadcmax();


typedef struct _action_t {
	char comm[STR_CMD_SIZE];
	void (*func) (void);
} action_t;


static const action_t actions[] = {
	{"status",     _cmd_status},
	{"saveadcmin", _cmd_saveadcmin},
	{"saveadcmax", _cmd_saveadcmax},
};
static const char TAG[] = "CMD";
static char buffer[2 * STR_CMD_SIZE] = { 0 };
static bool received = false;
static util_old_timer_t timer = { 0 };


void cmd_input(uint8_t byte)
{
	if (strlen(buffer) >= sizeof(buffer) - 1) {
		_clear();
	}
	if (byte == '\r' || byte == '\n') {
		return;
	}
	received = true;
	buffer[strlen(buffer)] = byte;
	util_old_timer_start(&timer, CMD_DELAY_MS);
}

void cmd_process()
{
	if (!received) {
		return;
	}

	if (received && !util_old_timer_wait(&timer)) {
		printTagLog(TAG, "unknown command: \"%s\"", buffer);
		_clear();
	}

	for (unsigned i = 0; i < __arr_len(actions); i++) {
		if (!strncmp(
				actions[i].comm,
				buffer,
				__min(
					strlen(actions[i].comm),
					sizeof(actions[i].comm) - 1
				)
			)
		) {
			printTagLog(TAG, "%s", buffer);
			actions[i].func();
			_clear();
			return;
		}
	}
}

void _clear()
{
	memset(buffer, 0, sizeof(buffer));
	received = false;
}

void _cmd_status()
{
	settings_show();
	pump_show_status();
	printTagLog(TAG, "ADC1: %d, ADC2: %u", get_system_adc(0), get_system_adc(1));
}

void _cmd_saveadcmin()
{
	settings.tank_ADC_min = get_level_adc();
	printTagLog(TAG, "New adc min value: %lu", settings.tank_ADC_min);
	set_status(NEED_SAVE_SETTINGS);
}

void _cmd_saveadcmax()
{
	settings.tank_ADC_max = get_level_adc();
	printTagLog(TAG, "New adc min value: %lu", settings.tank_ADC_max);
	set_status(NEED_SAVE_SETTINGS);
}
