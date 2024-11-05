/* Copyright © 2023 Georgy E. All rights reserved. */

#include "log.h"

#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdlib.h>

#include "soul.h"
#include "pump.h"
#include "glog.h"
#include "level.h"
#include "clock.h"
#include "fsm_gc.h"
#include "gutils.h"
#include "system.h"
#include "settings.h"
#include "pressure.h"
#include "sim_module.h"

#include "RecordDB.h"


#define SEND_DELAY_NS   (60 * SECOND_MS)
#define CHAR_PARAM_SIZE (30)


static bool _find_param(char** dst, const char* src, const char* param);
static void _make_record(RecordDB& record);
static bool _update_time(char* data);
static void _save_time_in_rtc_ram(uint32_t time_sec);
static void _clear_log();


static void _init_s(void);
static void _idle_s(void);
static void _check_net_s(void);
static void _send_s(void);

static void init_tims_a(void);
static void check_net_a(void);
static void check_timeout_a(void);
static void save_a(void);
static void send_a(void);
static void parse_a(void);
static void send_timeout_a(void);
static void error_a(void);


#if LOG_BEDUG
static const char* TAG                = "LOG";
#endif

static const char* T_DASH_FIELD       = "-";
static const char* T_TIME_FIELD       = "t";
static const char* T_COLON_FIELD      = ":";
static const char* TIME_FIELD         = "t";
static const char* CF_ID_FIELD        = "cf_id";
static const char* CF_DATA_FIELD      = "cf";
static const char* CF_PWR_FIELD       = "pwr";
static const char* CF_LTRMIN_FIELD    = "ltrmin";
static const char* CF_LTRMAX_FIELD    = "ltrmax";
static const char* CF_TRGT_FIELD      = "trgt";
static const char* CF_SLEEP_FIELD     = "sleep";
static const char* CF_SPEED_FIELD     = "speed";
static const char* CF_LOGID_FIELD     = "d_hwm";
static const char* CF_CLEAR_FIELD     = "clr";
static const char* CF_URL_FIELD       = "url";
static const char* CF_OUTA_FIELD      = "outa";
static const char* CF_OUTB_FIELD      = "outb";
static const char* CF_OUTC_FIELD      = "outc";
static const char* CF_OUTD_FIELD      = "outd";


FSM_GC_CREATE(log_fsm);

FSM_GC_CREATE_EVENT(success_e, 0);
FSM_GC_CREATE_EVENT(save_e,    0);
FSM_GC_CREATE_EVENT(send_e,    0);
FSM_GC_CREATE_EVENT(timeout_e, 0);
FSM_GC_CREATE_EVENT(error_e,   1);

FSM_GC_CREATE_STATE(init_s,      _init_s);
FSM_GC_CREATE_STATE(idle_s,      _idle_s);
FSM_GC_CREATE_STATE(check_net_s, _check_net_s);
FSM_GC_CREATE_STATE(send_s,      _send_s);

FSM_GC_CREATE_TABLE(
	log_fsm_table,
	{&init_s,      &success_e, &idle_s,      init_tims_a},

	{&idle_s,      &save_e,    &idle_s,      save_a},
	{&idle_s,      &send_e,    &check_net_s, check_net_a},

	{&check_net_s, &success_e, &send_s,      send_a},
	{&check_net_s, &timeout_e, &idle_s,      check_timeout_a},

	{&send_s,      &success_e, &idle_s,      parse_a},
	{&send_s,      &timeout_e, &idle_s,      send_timeout_a},
	{&send_s,      &error_e,   &idle_s,      error_a}
);


static util_old_timer_t timer = {};
static util_old_timer_t log_timer = {};
static util_old_timer_t send_timer = {};

static bool new_record_loaded = false;
static bool first_request = true;
static uint32_t sended_id = 0;
static RecordDB record(0);


void log_init()
{
	fsm_gc_init(&log_fsm, log_fsm_table, __arr_len(log_fsm_table));
}

void log_tick()
{
	fsm_gc_proccess(&log_fsm);
}

bool _find_param(char** dst, const char* src, const char* param)
{
	char search_param[CHAR_PARAM_SIZE] = {0};
	if (strlen(param) > sizeof(search_param) - 3) {
		return false;
	}

	char* ptr = NULL;

	snprintf(search_param, sizeof(search_param), "\n%s=", param);
	ptr = strnstr(src, search_param, strlen(src));
	if (ptr) {
		*dst = ptr + strlen(search_param);
		return true;
	}

	snprintf(search_param, sizeof(search_param), "=%s=", param);
	ptr = strnstr(src, search_param, strlen(src));
	if (ptr) {
		*dst = ptr + strlen(search_param);
		return true;
	}

	snprintf(search_param, sizeof(search_param), ";%s=", param);
	ptr = strnstr(src, search_param, strlen(src));
	if (ptr) {
		*dst = ptr + strlen(search_param);
		return true;
	}

	return false;
}

void _make_record(RecordDB& record)
{
	static const util_port_pin_t inputs[SETTINGS_INPUTS_CNT] = {
		{INPUT1_GPIO_Port, INPUT1_Pin},
		{INPUT2_GPIO_Port, INPUT2_Pin},
		{INPUT3_GPIO_Port, INPUT3_Pin},
		{INPUT4_GPIO_Port, INPUT4_Pin},
		{INPUT5_GPIO_Port, INPUT5_Pin},
		{INPUT6_GPIO_Port, INPUT6_Pin},
	};

	record.setRecordId(0);
	record.record.level         = get_level();
	record.record.press         = get_press();
	record.record.time          = get_clock_timestamp();
	record.record.pump_wok_time = settings.pump_work_sec;
	record.record.pump_downtime = settings.pump_downtime_sec;
	for (unsigned i = 0; i < __arr_len(inputs); i++) {
		HAL_GPIO_ReadPin(inputs[i].port, inputs[i].pin) ?
			__set_bit(record.record.inputs, i) :
			__reset_bit(record.record.inputs, i);
	}
}

bool _update_time(char* data)
{
	// Parse time
	clock_date_t date = {};
	clock_time_t time = {};

	char* data_ptr = data;
	if (!data_ptr) {
		return false;
	}
	date.Year = (uint16_t)(atoi(data_ptr));

	data_ptr = strnstr(data_ptr, T_DASH_FIELD, strlen(data_ptr));
	if (!data_ptr) {
		return false;
	}
	data_ptr += strlen(T_DASH_FIELD);
	date.Month = (uint8_t)atoi(data_ptr);

	data_ptr = strnstr(data_ptr, T_DASH_FIELD, strlen(data_ptr));
	if (!data_ptr) {
		return false;
	}
	data_ptr += strlen(T_DASH_FIELD);
	date.Date = (uint8_t)atoi(data_ptr);

	if (!save_clock_date(&date)) {
		return false;
	}

	data_ptr = strnstr(data_ptr, T_TIME_FIELD, strlen(data_ptr));
	if (!data_ptr) {
		return false;
	}
	data_ptr += strlen(T_TIME_FIELD);
	time.Hours = (uint8_t)atoi(data_ptr);

	data_ptr = strnstr(data_ptr, T_COLON_FIELD, strlen(data_ptr));
	if (!data_ptr) {
		return false;
	}
	data_ptr += strlen(T_COLON_FIELD);
	time.Minutes = (uint8_t)atoi(data_ptr);

	data_ptr = strnstr(data_ptr, T_COLON_FIELD, strlen(data_ptr));
	if (!data_ptr) {
		return false;
	}
	data_ptr += strlen(T_COLON_FIELD);
	time.Seconds = (uint8_t)atoi(data_ptr);

	if(save_clock_time(&time)) {
		set_clock_ready(true);
		return true;
	}

	return false;
}

void _save_time_in_rtc_ram(uint32_t time_sec)
{
	for (uint8_t i = sizeof(time_sec); i > 0; i--) {
		uint8_t byte = (uint8_t)(time_sec & 0xFF);
		if (!set_clock_ram(i - 1, byte)) {
			return;
		}
		time_sec >>= BITS_IN_BYTE;
	}
}

void _clear_log()
{
	settings.server_log_id = 0;
	settings.cf_id = 0;
	settings.pump_work_sec = 0;
	settings.pump_work_day_sec = 0;
	settings.pump_downtime_sec = 0;
	set_status(NEED_SAVE_SETTINGS);
}

void _init_s(void)
{
	fsm_gc_push_event(&log_fsm, &success_e);
}

void _idle_s(void)
{
	if (!util_old_timer_wait(&log_timer) && is_status(DS1307_READY)) {
		fsm_gc_push_event(&log_fsm, &save_e);
	}

	if (!util_old_timer_wait(&send_timer)) {
		fsm_gc_push_event(&log_fsm, &send_e);
	}
}

void _check_net_s(void)
{
	if (if_network_ready()) {
		fsm_gc_push_event(&log_fsm, &success_e);
	}

	if (!util_old_timer_wait(&timer)) {
		fsm_gc_push_event(&log_fsm, &timeout_e);
	}
}

void _send_s(void)
{
	if (has_http_response()) {
		fsm_gc_push_event(&log_fsm, &success_e);
	}

	if (!util_old_timer_wait(&timer)) {
		fsm_gc_push_event(&log_fsm, &timeout_e);
	}
}


void init_tims_a(void)
{
	fsm_gc_clear(&log_fsm);

	uint32_t sleep_sec = settings.sleep_ms / SECOND_MS;
	uint32_t rtc_ram_last_sec = 0;
	uint8_t  byte = 0;
	for (uint8_t i = 0; i < sizeof(rtc_ram_last_sec); i++) {
		rtc_ram_last_sec <<= BITS_IN_BYTE;
		bool res = get_clock_ram(i, &byte);
		if (res) {
			rtc_ram_last_sec |= ((uint32_t)byte & 0xFF);
		} else {
			rtc_ram_last_sec = 0;
			break;
		}
	}
	uint32_t timestamp = get_clock_timestamp();
	if (rtc_ram_last_sec == 0xFFFFFFFF) {
		sleep_sec = 1;
	} else if (timestamp >= rtc_ram_last_sec + sleep_sec) {
		sleep_sec = 1;
	} else {
		sleep_sec -= timestamp - rtc_ram_last_sec;
	}
	util_old_timer_start(&log_timer, sleep_sec * SECOND_MS);

	util_old_timer_start(&send_timer, GENERAL_TIMEOUT_MS);
}

void check_net_a(void)
{
	util_old_timer_start(&timer, 5 * SECOND_MS);
}

void save_a(void)
{
	_make_record(record);

	if (record.save() == RecordDB::RECORD_OK) {
		settings.pump_work_sec = 0;
		settings.pump_downtime_sec = 0;
		set_status(NEED_SAVE_SETTINGS);
		reset_status(NEW_RECORD_WAS_NOT_SAVED);
		_save_time_in_rtc_ram(record.record.time);
		util_old_timer_start(&log_timer, settings.sleep_ms);
	} else {
		set_status(NEW_RECORD_WAS_NOT_SAVED);
		util_old_timer_start(&log_timer, GENERAL_TIMEOUT_MS);
	}
}

void check_timeout_a(void)
{
	util_old_timer_start(&send_timer, 10 * SECOND_MS);
}

void send_a(void)
{
	bool is_base_server = strncmp(get_sim_url(), settings.url, strlen(settings.url));

	char data[SIM_LOG_SIZE] = {};
	snprintf(
		data,
		sizeof(data),
		"id=%s\n"
		"fw_id=%u\n"
		"cf_id=%lu\n",
		get_system_serial_str(),
		FW_VERSION,
		is_base_server ? 0 : settings.cf_id
	);
	if (!settings.calibrated) {
		snprintf(
			data + strlen(data),
			sizeof(data) - strlen(data),
			"adclevel=%lu\n",
			get_level_adc()
		);
	}
	snprintf(
		data + strlen(data),
		sizeof(data) - strlen(data),
		"t=%s\n",
		get_clock_time_format()
	);

	RecordDB::RecordStatus recordStatus = RecordDB::RECORD_ERROR;
	if (!new_record_loaded && is_status(HAS_NEW_RECORD)) {
		record.setRecordId(settings.server_log_id);
		recordStatus = record.loadNext();
	}
	if (recordStatus == RecordDB::RECORD_NO_LOG) {
		reset_status(HAS_NEW_RECORD);
	} else if (recordStatus != RecordDB::RECORD_OK) {
#if LOG_BEDUG
		printTagLog(TAG, "error load record");
#endif
	}
	if (!first_request && is_status(NEW_RECORD_WAS_NOT_SAVED)) {
		util_old_timer_start(&log_timer, settings.sleep_ms);
		reset_status(NEW_RECORD_WAS_NOT_SAVED);
		recordStatus = RecordDB::RECORD_OK;
		_make_record(record);
		_save_time_in_rtc_ram(record.record.time);
		record.record.id = settings.server_log_id + 1;
	}
	if (// settings.calibrated &&
		recordStatus == RecordDB::RECORD_OK &&
		!is_base_server
	) {
		snprintf(
			data + strlen(data),
			sizeof(data) - strlen(data),
			"d="
				"id=%lu;"
				"t=%s;"
				"level=%ld;"
				"press=%u.%02u;"
				"pumpw=%lu;"
				"inp1=%u;"
				"inp2=%u;"
				"inp3=%u;"
				"inp4=%u;"
				"inp5=%u;"
				"inp6=%u;"
				"pumpd=%lu\r\n",
			record.record.id,
			get_clock_time_format_by_sec(record.record.time),
			record.record.level,
			record.record.press / 100, record.record.press % 100,
			record.record.pump_wok_time,
			(unsigned)__get_bit(record.record.inputs, 0),
			(unsigned)__get_bit(record.record.inputs, 1),
			(unsigned)__get_bit(record.record.inputs, 2),
			(unsigned)__get_bit(record.record.inputs, 3),
			(unsigned)__get_bit(record.record.inputs, 4),
			(unsigned)__get_bit(record.record.inputs, 5),
			record.record.pump_downtime
		);
		new_record_loaded = true;
		sended_id = record.record.id;
	} else {
		sended_id = 0;
	}

	if (is_status(DS1307_READY)) {
		new_record_loaded = false;
	}


#if LOG_BEDUG
	printTagLog(TAG, "request:\n%s", data);
#endif
	send_sim_http_post(data);

	util_old_timer_start(&timer,      30 * SECOND_MS);
	util_old_timer_start(&send_timer, 10 * SECOND_MS);
}

void parse_a(void)
{
	fsm_gc_clear(&log_fsm);

	char* var_ptr = get_response();
	char* data_ptr = var_ptr;

#if LOG_BEDUG
	printTagLog(TAG, "response: %s", var_ptr);
#endif

	if (!var_ptr) {
#if LOG_BEDUG
		printTagLog(TAG, "unable to parse response (no response)");
#endif
		return;
	}

	if (!_find_param(&data_ptr, var_ptr, TIME_FIELD)) {
#if LOG_BEDUG
		printTagLog(TAG, "unable to parse response (no time) - [%s]", var_ptr);
#endif
		return;
	}

	if (_update_time(data_ptr)) {
#if LOG_BEDUG
		printTagLog(TAG, "time updated\n");
#endif
	} else {
#if LOG_BEDUG
		printTagLog(TAG, "unable to parse response (unable to update time) - [%s]", var_ptr);
#endif
		return;
	}

	// Parse configuration:
	if (!_find_param(&data_ptr, var_ptr, CF_LOGID_FIELD)) {
#if LOG_BEDUG
		printTagLog(TAG, "unable to parse response (log_id not found) - %s", var_ptr);
#endif
		return;
	}
	settings.server_log_id = atoi(data_ptr);
	if (sended_id && sended_id < settings.server_log_id) {
		util_old_timer_start(&log_timer, GENERAL_TIMEOUT_MS);
	}
	first_request = false;

#if LOG_BEDUG
	printTagLog(TAG, "Recieved response from the server\n");
#endif

	if (!_find_param(&data_ptr, var_ptr, CF_ID_FIELD)) {
#if LOG_BEDUG
		printTagLog(TAG, "unable to parse response (cf_id not found) - %s", var_ptr);
#endif
	}
	settings.cf_id = atoi(data_ptr);

	if (!_find_param(&data_ptr, var_ptr, CF_DATA_FIELD)) {
#if LOG_BEDUG
		printTagLog(TAG, "warning: no cf_id data - [%s]", var_ptr);
#endif
	}

	if (_find_param(&data_ptr, var_ptr, CF_PWR_FIELD)) {
		pump_update_enable_state(atoi(data_ptr));
	}

	if (_find_param(&data_ptr, var_ptr, CF_LTRMIN_FIELD)) {
		pump_update_ltrmin(atoi(data_ptr));
	}

	if (_find_param(&data_ptr, var_ptr, CF_LTRMAX_FIELD)) {
		pump_update_ltrmax(atoi(data_ptr));
	}

	if (_find_param(&data_ptr, var_ptr, CF_TRGT_FIELD)) {
		pump_update_target(atoi(data_ptr));
	}

	if (_find_param(&data_ptr, var_ptr, CF_SLEEP_FIELD)) {
		set_settings_sleep(atoi(data_ptr) * SECOND_MS);
	}

	if (_find_param(&data_ptr, var_ptr, CF_SPEED_FIELD)) {
		pump_update_speed(atoi(data_ptr));
	}

	if (_find_param(&data_ptr, var_ptr, CF_CLEAR_FIELD)) {
		if (atoi(data_ptr) == 1) {
			_clear_log();
		}
	}

	if (_find_param(&data_ptr, var_ptr, CF_OUTA_FIELD)) {
		settings.outputs[0] = atoi(data_ptr) ? 1 : 0;
	}
	if (_find_param(&data_ptr, var_ptr, CF_OUTB_FIELD)) {
		settings.outputs[1] = atoi(data_ptr) ? 1 : 0;
	}
	if (_find_param(&data_ptr, var_ptr, CF_OUTC_FIELD)) {
		settings.outputs[2] = atoi(data_ptr) ? 1 : 0;
	}
	if (_find_param(&data_ptr, var_ptr, CF_OUTD_FIELD)) {
		settings.outputs[3] = atoi(data_ptr) ? 1 : 0;
	}

	if (_find_param(&data_ptr, var_ptr, CF_URL_FIELD)) {
		char url[CHAR_SETIINGS_SIZE] = "";
		for (unsigned i = 0; i < __min(strlen(data_ptr), sizeof(url) - 1); i++) {
			if (data_ptr[i] == ';' ||
				isspace(data_ptr[i])
			) {
				break;
			}
			url[i] = data_ptr[i];
		}
		set_settings_url(url);
	}

#if LOG_BEDUG
	printTagLog(TAG, "configuration updated");
#endif
	settings_show();
	set_status(NEED_SAVE_SETTINGS);

	util_old_timer_start(&send_timer, SEND_DELAY_NS);
}

void send_timeout_a(void)
{
	fsm_gc_clear(&log_fsm);

	util_old_timer_start(&send_timer, 10 * SECOND_MS);
}

void error_a(void)
{
	fsm_gc_clear(&log_fsm);

	util_old_timer_start(&send_timer, SEND_DELAY_NS);
}
