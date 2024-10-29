/* Copyright © 2023 Georgy E. All rights reserved. */

#include "settings.h"

#include <stdio.h>
#include <string.h>

#include "glog.h"
#include "main.h"
#include "gutils.h"
#include "clock.h"
#include "system.h"
#include "hal_defs.h"


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

	return true;
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
	other->dv_type  = DEVICE_TYPE;
	other->fw_id    = FW_VERSION;
	other->sw_id    = SW_VERSION;
	other->cf_id    = CF_VERSION;

	memset(other->url, 0, sizeof(other->url));
	strncpy(other->url, defaultUrl, sizeof(other->url));
}

void settings_show()
{
#if SETTINGS_BEDUG
	gprint(
		"\n####################SETTINGS####################\n"
		"Time:             %s\n"
		"Device ID:        %s\n"
		"Server URL:       %s\n"
		"####################SETTINGS####################\n",
		get_clock_time_format(),
		get_system_serial_str(),
		settings.url
	);
#else
    gprint("####################SETTINGS####################\n");
    gprint("Time:             %s\n",       get_clock_time_format());
    gprint("Device ID:        %s\n",       get_system_serial_str());
    gprint("Server URL:       %s\n",       settings.url);
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
