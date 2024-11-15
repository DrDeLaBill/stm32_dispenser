/* Copyright © 2023 Georgy E. All rights reserved. */

#ifndef _SETTINGS_H_
#define _SETTINGS_H_


#ifdef __cplusplus
extern "C" {
#endif


#include <stdint.h>
#include <stdbool.h>

#include "main.h"


#ifdef DEBUG
#   define SETTINGS_BEDUG (1)
#endif


#define DEVICE_MAJOR (2)
#define DEVICE_MINOR (1)
#define DEVICE_PATCH (0)


/*
 * Device types:
 * 0x0001 - Dispenser
 * 0x0002 - Gas station
 * 0x0003 - Logger
 * 0x0004 - B.O.B.A.
 * 0x0005 - Calibrate station
 * 0x0006 - Dispenser-mini
 */
#define DEVICE_TYPE           ((uint16_t)0x0006)
#define SW_VERSION            ((uint8_t)0x01)
#define FW_VERSION            ((uint8_t)0x01)
#define CF_VERSION            ((uint8_t)0x01)
#define CHAR_SETIINGS_SIZE    (30)

#define BEDACODE              ((uint32_t)0xBEDAC0DE)


#define MILLILITERS_IN_LITER  (1000)

#define SETTINGS_OUTPUTS_CNT  (4)
#define SETTINGS_INPUTS_CNT   (6)


typedef enum _SettingsStatus {
    SETTINGS_OK = 0,
    SETTINGS_ERROR
} SettingsStatus;


typedef struct __attribute__((packed)) _settings_t  {
	uint32_t bedacode;
	// Device type
	uint16_t dv_type;
	// Software version
    uint8_t  sw_id;
    // Firmware version
    uint8_t  fw_id;
	// Configuration version
	uint32_t cf_id;
	// Remote server url
	char     url[CHAR_SETIINGS_SIZE];
	// Enable pump
	uint8_t  pump_enabled;
	// Measure delay in milliseconds
	uint32_t sleep_ms;
	// Current server log ID
	uint32_t server_log_id;
	// Liters ADC value when liquid tank can be considered full
	uint32_t tank_ADC_min;
	// Liters ADC value when liquid tank can be considered empty
	uint32_t tank_ADC_max;
	// Liters value when liquid tank can be considered full
	uint32_t tank_ltr_max;
	// Liters value when liquid tank can be considered empty
	uint32_t tank_ltr_min;
	// Target milliliters per day for pump
	uint32_t pump_target_ml;
	// Pump speed: milliliters per hour
	uint32_t pump_speed;
	// Current pump work sec
	uint32_t pump_work_sec;
	// Current day pump downtime sec
	uint32_t pump_downtime_sec;
	// Pump work sec for current day
	uint32_t pump_work_day_sec;
	// Current log day
	uint8_t  pump_log_date;
	// Registrated on server
	uint8_t  registrated;
	// Is calibrated
	uint8_t  calibrated;
	// Outputs states
	uint8_t  outputs[SETTINGS_OUTPUTS_CNT];
} settings_t;


extern settings_t settings;


extern const char defaultUrl[CHAR_SETIINGS_SIZE];


/* copy settings to the target */
settings_t* settings_get();
/* copy settings from the target */
void settings_set(settings_t* other);

uint32_t settings_size();

bool settings_check(settings_t* other);
void settings_repair(settings_t* other);
void settings_reset(settings_t* other);

void settings_show();

void set_settings_url(const char* url);
void set_settings_sleep(uint32_t sleep);

void settings_update();


#ifdef __cplusplus
}
#endif


#endif
