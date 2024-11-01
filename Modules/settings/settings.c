/* Copyright © 2023 Georgy E. All rights reserved. */

#include <clock.h>
#include "settings.h"

#include <stdio.h>
#include <string.h>

#include "glog.h"
#include "main.h"
#include "gutils.h"
#include "system.h"
#include "hal_defs.h"


#define CHAR_PARAM_SIZE       30
#define DEFAULT_SLEEPING_TIME 900000
#define MIN_TANK_VOLUME       3126
#define MAX_TANK_VOLUME       75
#define MIN_TANK_LTR          10000
#define MAX_TANK_LTR          375000


#if SETTINGS_BEDUG
static const char SETTINGS_TAG[] = "STNG";
#endif

const char defaultUrl[CHAR_SETIINGS_SIZE]  = "urv.iot.turtton.ru";

settings_t settings = { 0 };


settings_t* settings_get()
{
	return &settings;
}

void settings_set(settings_t* other)
{
	memcpy((uint8_t*)&settings, (uint8_t*)other, sizeof(settings));
}

uint32_t settings_size()
{
	return sizeof(settings_t);
}

bool settings_check(settings_t* other)
{
	if (other->bedacode != BEDACODE) {
		return false;
	}
	if (other->dv_type != DEVICE_TYPE) {
		return false;
	}
	if (other->fw_id != FW_VERSION) {
		return false;
	}
	if (other->sw_id != SW_VERSION) {
		return false;
	}

	return other->sleep_ms > 0;
}

void settings_repair(settings_t* other)
{
	if (!settings_check(other)) {
		settings_reset(other);
	}
}

void settings_reset(settings_t* other)
{
#if SETTINGS_BEDUG
	printTagLog(SETTINGS_TAG, "Reset settings");
#endif

	other->bedacode = BEDACODE;
	other->dv_type = DEVICE_TYPE;
	other->fw_id = FW_VERSION;
	other->sw_id = SW_VERSION;

	other->cf_id = CF_VERSION;
	set_settings_url(defaultUrl);
	other->tank_ltr_min = MIN_TANK_LTR;
	other->tank_ltr_max = MAX_TANK_LTR;
	other->tank_ADC_max = MAX_TANK_VOLUME;
	other->tank_ADC_min = MIN_TANK_VOLUME;
	other->pump_target_ml = 0;
	other->pump_speed = 0;
	other->sleep_ms = DEFAULT_SLEEPING_TIME;
	other->pump_enabled = true;
	other->server_log_id = 0;
	other->pump_work_day_sec = 0;
	other->pump_downtime_sec = 0;
	other->pump_work_sec = 0;
	other->pump_log_date = get_clock_date();
	other->registrated = 0;
	other->calibrated = 0;

	memset(other->outputs, 0, sizeof(other->outputs));
}

void settings_show()
{
#if SETTINGS_BEDUG
	gprint(
		"\n####################SETTINGS####################\n"
		"Time:             %s\n"
		"Device ID:        %s\n"
		"Server URL:       %s\n"
		"Sleep time:       %lu sec\n"
		"Target:           %lu l/d\n"
		"Pump speed:       %lu ml/h\n"
		"Pump work:        %lu sec\n"
		"Pump work day:    %lu sec\n"
		"Pump              %s\n"
		"ADC level MIN:    %lu\n"
		"ADC level MAX:    %lu\n"
		"Liquid level MIN: %lu l\n"
		"Liquid level MAX: %lu l\n"
		"Server log ID:    %lu\n"
		"Config ver:       %lu\n"
		"Outputs:          A-%u,B-%u,C-%u,D-%u\n"
		"####################SETTINGS####################\n",
		get_clock_time_format(),
		get_system_serial_str(),
		settings.url,
		settings.sleep_ms / SECOND_MS,
		settings.pump_target_ml / SECOND_MS,
		settings.pump_speed,
		settings.pump_work_sec,
		settings.pump_work_day_sec,
		settings.pump_enabled ? "ON" : "OFF",
		settings.tank_ADC_min,
		settings.tank_ADC_max,
		settings.tank_ltr_min,
		settings.tank_ltr_max,
		settings.server_log_id,
		settings.cf_id,
		settings.outputs[0], settings.outputs[1], settings.outputs[2], settings.outputs[3]
	);
#else
    gprint("####################SETTINGS####################\n");
    gprint("Time:             %s\n",       get_clock_time_format());
    gprint("Device ID:        %s\n",       get_system_serial_str());
    gprint("Server URL:       %s\n",       settings.url);
    gprint("Sleep time:       %lu sec\n",  settings.sleep_ms / SECOND_MS);
    gprint("Target:           %lu l/d\n",  settings.pump_target_ml / SECOND_MS);
    gprint("Pump speed:       %lu ml/h\n", settings.pump_speed);
    gprint("Pump work:        %lu sec\n",  settings.pump_work_sec);
    gprint("Pump work day:    %lu sec\n",  settings.pump_work_day_sec);
    gprint("Pump:             %s\n",       settings.pump_enabled ? "ON" : "OFF");
    gprint("Outputs:          A-%u,B-%u,C-%u,D-%u\n", settings.outputs[0], settings.outputs[1], settings.outputs[2], settings.outputs[3]);
    gprint("####################SETTINGS####################\n");
#endif
}

void set_settings_url(const char* url)
{
	if (!url) {
		return;
	}
	if (!strlen(url)) {
		return;
	}
	memset(settings.url, 0, sizeof(settings.url));
	strncpy(settings.url, url, __min(sizeof(settings.url) - 1, strlen(url)));
}

void set_settings_sleep(uint32_t sleep)
{
	if (sleep) {
		settings.sleep_ms = sleep;
	}
}
