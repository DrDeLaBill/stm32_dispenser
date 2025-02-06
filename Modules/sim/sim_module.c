/* Copyright © 2025 Georgy E. All rights reserved. */

#include "sim_module.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <strings.h>
#include <ctype.h>

#include "glog.h"
#include "main.h"
#include "fsm_gc.h"
#include "gutils.h"
#include "settings.h"


#define SIM_MAX_ERRORS     (5)
#define SIM_DELAY_MS       (10000)
#define SIM_HTTP_SIZE      (90)
#define SIM_AFTER_RESET_MS (15000)
#define SIM_WAIT_USER_MS   (MINUTE_MS)


extern settings_t settings;


const char* SIM_TAG = "SIM";

const char* SUCCESS_CMD_RESP  = "ok";
const char* SUCCESS_HTTP_ACT  = "+chttpact: request";
const char* HTTP_ACT_COUNT    = "+chttpact: ";
const char* CONTENT_LENGTH    = "content-length: ";
const char* SUCCESS_HTTP_RESP = "200 ok";
const char* LINE_BREAK        = "\r\n";
const char* DOUBLE_LINE_BREAK = "\r\n\r\n";
const char* SIM_ERR_RESPONSE  = "\r\nerror\r\n";


typedef enum _sim_status_t {
	SIM_OK = 0,
	SIM_WAIT,
	SIM_TIMEOUT
} sim_status_t;

typedef struct _sim_command_t {
    char request [30];
    char response[7];
} sim_command_t;


typedef struct _sim_t {
	bool     initialized;
	bool     done;
	unsigned counter;

	char     url[CHAR_SETIINGS_SIZE];

	char     request[SIM_LOG_SIZE];
	char     response[RESPONSE_SIZE];
	unsigned resp_cnt;
	unsigned resp_len;

	unsigned errors;

	gtimer_t timer;

	bool     is_base_server;
	bool     http_error;
} sim_t;


sim_t sim = {0};

static const char httpaction[] = "+httpaction: 1,200,";
static const char contentlength[] = "content-length: ";
static const sim_command_t start_cmds[] = {
	{"AT",                                "ok"},
	{"ATE0",                              "ok"},
	{"AT+CSQ",                            "ok"},
	{"AT+CGMR",                           "sim868"},
	{"AT+COPS?",                          "ok"},
	{"AT+CCLK?",                          "ok"},
	{"AT+SAPBR=2,1",                      "ok"},
	{"AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\"", "ok"},
	{"AT+SAPBR=3,1,\"APN\",\"internet\"", "ok"},
	{"AT+SAPBR=1,1",                      "ok"},
};


static void _sim_send_cmd(const char* cmd);
static void _sim_clear_response();
static bool _sim_validate(const char* target);
static void _simple_state_s(const char* resp);
static void _simple_action_a(const char* req);


FSM_GC_CREATE(sim_fsm)

FSM_GC_CREATE_EVENT(end_e,      0)
FSM_GC_CREATE_EVENT(change_e,   0)
FSM_GC_CREATE_EVENT(success_e,  0)
FSM_GC_CREATE_EVENT(timeout_e,  1)
FSM_GC_CREATE_EVENT(error_e,    2)

FSM_GC_CREATE_STATE(init_s,           _init_s)
FSM_GC_CREATE_STATE(reset_s,          _reset_s)
FSM_GC_CREATE_STATE(after_reset_s,    _after_reset_s)
FSM_GC_CREATE_STATE(start_s,          _start_s)
FSM_GC_CREATE_STATE(init_http_s,      _init_http_s)
FSM_GC_CREATE_STATE(start_http_s,     _start_http_s)
FSM_GC_CREATE_STATE(send_wait_s,      _send_wait_s)
FSM_GC_CREATE_STATE(send_http_s,      _send_http_s)
FSM_GC_CREATE_STATE(send_post_s,      _send_post_s)
FSM_GC_CREATE_STATE(wait_http_200_s,  _wait_http_200_s)
FSM_GC_CREATE_STATE(wait_post_s,      _wait_post_s)
FSM_GC_CREATE_STATE(read_length_s,    _read_length_s)
FSM_GC_CREATE_STATE(read_data_s,      _read_data_s)
FSM_GC_CREATE_STATE(wait_data_s,      _wait_data_s)
FSM_GC_CREATE_STATE(wait_user_s,      _wait_user_s)
FSM_GC_CREATE_STATE(close_http_s,     _close_http_s)
FSM_GC_CREATE_STATE(count_error_s,    _count_error_s)

FSM_GC_CREATE_ACTION(reset_a,          _reset_a)
FSM_GC_CREATE_ACTION(after_reset_a,    _after_reset_a)
FSM_GC_CREATE_ACTION(close_http_err_a, _close_http_err_a)
FSM_GC_CREATE_ACTION(start_a,          _start_a)
FSM_GC_CREATE_ACTION(start_iterate_a,  _start_iterate_a)
FSM_GC_CREATE_ACTION(init_http_a,      _init_http_a)
FSM_GC_CREATE_ACTION(change_url_a,     _change_url_a)
FSM_GC_CREATE_ACTION(close_http_a,     _close_http_a)
FSM_GC_CREATE_ACTION(start_http_a,     _start_http_a)
FSM_GC_CREATE_ACTION(send_wait_a,      _send_wait_a)
FSM_GC_CREATE_ACTION(send_http_a,      _send_http_a)
FSM_GC_CREATE_ACTION(send_post_a,      _send_post_a)
FSM_GC_CREATE_ACTION(wait_http_200_a,  _wait_http_200_a)
FSM_GC_CREATE_ACTION(wait_post_a,      _wait_post_a)
FSM_GC_CREATE_ACTION(read_data_a,      _read_data_a)
FSM_GC_CREATE_ACTION(read_length_a,    _read_length_a)
FSM_GC_CREATE_ACTION(wait_data_a,      _wait_data_a)
FSM_GC_CREATE_ACTION(wait_user_a,      _wait_user_a)
FSM_GC_CREATE_ACTION(count_error_a,    _count_error_a)

FSM_GC_CREATE_TABLE(
	sim_fsm_table,
	{&init_s,           &success_e,  &reset_s,         &reset_a},

	{&start_s,          &success_e,  &start_s,         &start_iterate_a},
	{&start_s,          &end_e,      &init_http_s,     &init_http_a},
	{&start_s,          &timeout_e,  &count_error_s,   &count_error_a},

	{&init_http_s,      &success_e,  &start_http_s,    &start_http_a},
	{&init_http_s,      &timeout_e,  &close_http_s,    &close_http_err_a},

	{&start_http_s,     &success_e,  &send_wait_s,     &send_wait_a},
	{&start_http_s,     &timeout_e,  &close_http_s,    &close_http_err_a},

	{&send_wait_s,      &success_e,  &send_http_s,     &send_http_a},

	{&send_http_s,      &success_e,  &send_post_s,     &send_post_a},
	{&send_http_s,      &timeout_e,  &close_http_s,    &close_http_err_a},

	{&send_post_s,      &success_e,  &wait_http_200_s, &wait_http_200_a},
	{&send_post_s,      &timeout_e,  &close_http_s,    &close_http_err_a},

	{&wait_http_200_s,  &success_e,  &wait_post_s,     &wait_post_a},
	{&wait_http_200_s,  &timeout_e,  &close_http_s,    &close_http_err_a},

	{&wait_post_s,      &success_e,  &read_length_s,   &read_length_a},
	{&wait_post_s,      &timeout_e,  &close_http_s,    &close_http_err_a},

	{&read_length_s,    &success_e,  &read_data_s,     &read_data_a},
	{&read_length_s,    &timeout_e,  &close_http_s,    &close_http_err_a},

	{&read_data_s,      &success_e,  &wait_data_s,     &wait_data_a},
	{&read_data_s,      &timeout_e,  &close_http_s,    &close_http_err_a},

	{&wait_data_s,      &success_e,  &wait_user_s,     &wait_user_a},
	{&wait_data_s,      &timeout_e,  &close_http_s,    &close_http_err_a},

	{&wait_user_s,      &success_e,  &close_http_s,    &close_http_a},

	{&close_http_s,     &success_e,  &init_http_s,     &init_http_a},
	{&close_http_s,     &change_e,   &count_error_s,   &change_url_a},
	{&close_http_s,     &error_e,    &count_error_s,   &count_error_a},

	{&count_error_s,    &success_e,  &start_s,         &start_a},
	{&count_error_s,    &error_e,    &reset_s,         &reset_a},

	{&reset_s,          &success_e,  &after_reset_s,   &after_reset_a},

	{&after_reset_s,    &success_e,  &start_s,         &start_a},
)


void sim_process()
{
	if (!sim.initialized) {
		fsm_gc_init(&sim_fsm, sim_fsm_table, __arr_len(sim_fsm_table));
		sim.initialized = true;
	}
	fsm_gc_process(&sim_fsm);
}

void sim_proccess_input(const char input_chr)
{
    sim.response[sim.resp_cnt++] = (uint8_t)tolower(input_chr);
    if (sim.resp_cnt >= sizeof(sim.response) - 1) {
		_sim_clear_response();
    }
    sim.response[sim.resp_cnt] = 0;
}

void send_sim_http_post(const char* data)
{
    if (!if_network_ready() || sim.done) {
        return;
    }
    memcpy(sim.request, data, sizeof(sim.request) - 1);
    sim.request[__min(strlen(sim.request), sizeof(sim.request) - 2)] = END_OF_STRING;
    sim.done = true;
}

char* get_response()
{
	sim.done = true;
    return sim.response;
}

char* get_sim_url()
{
	return sim.url;
}

bool is_base_server()
{
	return sim.is_base_server;
}

void set_base_server()
{
    strncpy(sim.url, defaultUrl, sizeof(sim.url));
    sim.is_base_server = true;
}

void set_main_server()
{
    memcpy(sim.url, settings.url, sizeof(sim.url));
    sim.is_base_server = false;
}

bool if_network_ready()
{
	return fsm_gc_is_state(&sim_fsm, &send_wait_s);
}

bool has_http_response()
{
    return fsm_gc_is_state(&sim_fsm, &wait_user_s);
}

void _sim_send_cmd(const char* cmd)
{
	_sim_clear_response();
    HAL_UART_Transmit(&SIM_MODULE_UART, (uint8_t*)cmd, (uint16_t)strlen(cmd), GENERAL_TIMEOUT_MS);
    HAL_UART_Transmit(&SIM_MODULE_UART, (uint8_t*)LINE_BREAK, (uint16_t)strlen(LINE_BREAK), GENERAL_TIMEOUT_MS);
#if SIM_MODULE_DEBUG
    printTagLog(SIM_TAG, "send - %s\n", cmd);
#endif
}

bool _sim_validate(const char* target)
{
    if (strnstr(sim.response, target, strlen(sim.response)) != NULL) {
#if SIM_MODULE_DEBUG
        printTagLog(SIM_TAG, "success - [%s]\n", sim.response);
#endif
        return true;
    }
    return false;
}

void _simple_action_a(const char* req)
{
	fsm_gc_clear(&sim_fsm);
	_sim_send_cmd(req);
	gtimer_start(&sim.timer, SIM_DELAY_MS);
}

void _simple_state_s(const char* resp)
{
	if (_sim_validate(resp)) {
		fsm_gc_push_event(&sim_fsm, &success_e);
	}
	if (gtimer_wait(&sim.timer)) {
		return;
	}
	fsm_gc_push_event(&sim_fsm, &timeout_e);
}

void _sim_clear_response()
{
    sim.response[0] = 0;
    sim.resp_cnt = 0;
}

void _init_s(void)
{
	memcpy(sim.url, settings.url, strlen(sim.url));
	fsm_gc_push_event(&sim_fsm, &success_e);
}

void _start_a(void)
{
	_simple_action_a(start_cmds[sim.counter].request);
}

void _start_iterate_a(void)
{
	sim.counter++;
	if (sim.counter < __arr_len(start_cmds)) {
		_simple_action_a(start_cmds[sim.counter].request);
	}
}

void _start_s(void)
{
	if (sim.counter >= __arr_len(start_cmds)) {
		fsm_gc_push_event(&sim_fsm, &end_e);
	}
	_simple_state_s(start_cmds[sim.counter].response);
}

void _init_http_a(void)
{
	sim.http_error = false;
	_simple_action_a("AT+HTTPINIT");
}

void _init_http_s(void)
{
	_simple_state_s("ok");
}

void _start_http_a(void)
{
	snprintf(
		sim.request,
		sizeof(sim.request),
		"AT+HTTPPARA=\"URL\",\"http://%s/api/log/ep\"",
		sim.url
	);
	_simple_action_a(sim.request);
}

void _start_http_s(void)
{
	_simple_state_s("ok");
}

void _send_wait_a(void)
{
	sim.done = false;
}

void _send_wait_s(void)
{
	if (!sim.done) {
		return;
	}
	fsm_gc_push_event(&sim_fsm, &success_e);
}

void _send_http_a(void)
{
	char httpdata[35] = "";
	sim.done = false;
	snprintf(
		httpdata,
		sizeof(httpdata),
		"AT+HTTPDATA=%d,%d",
		strlen(sim.request),
		1000
	);
	_simple_action_a(httpdata);
}

void _send_http_s(void)
{
	_simple_state_s("download");
}

void _send_post_a(void)
{
	_simple_action_a(sim.request);
}

void _send_post_s(void)
{
	_simple_state_s("ok");
}

void _wait_http_200_a(void)
{
	_simple_action_a("AT+HTTPACTION=1");
}

void _wait_http_200_s(void)
{
	_simple_state_s(httpaction);
}

void _wait_post_a(void)
{
	gtimer_start(&sim.timer, SIM_DELAY_MS);
}

void _wait_post_s(void)
{
	char* ptr = strnstr(
			sim.response,
			httpaction,
			sizeof(sim.response)
		) + strlen(httpaction);
	char format[32] = {0};
	snprintf(
		format,
		sizeof(format) - 1,
		"%s%u\r\n",
		httpaction,
		(unsigned)atoi(ptr)
	);
	sim.resp_len = (unsigned)atoi(ptr);
	_simple_state_s(format);
}

void _read_length_a(void)
{
	_simple_action_a("AT+HTTPHEAD");
}

void _read_length_s(void)
{
	_simple_state_s(contentlength);
}

void _read_data_a(void)
{
	gtimer_start(&sim.timer, SIM_DELAY_MS);
}

void _read_data_s(void)
{
	char* ptr = strnstr(
			sim.response,
			contentlength,
			sizeof(sim.response)
		) + strlen(contentlength);
	char format[32] = {0};
	snprintf(format, sizeof(format) - 1, "%s%u\r\n", contentlength, (unsigned)atoi(ptr));

	sim.resp_len = (unsigned)atoi(ptr);
	_simple_state_s(format);
}

void _wait_data_a(void)
{
	snprintf(sim.request, sizeof(sim.request), "AT+HTTPREAD=0,%u", sim.resp_len);
	_simple_action_a(sim.request);
}

void _wait_data_s(void)
{
	_simple_state_s("ok");
}

void _wait_user_a(void)
{
	gtimer_start(&sim.timer, SIM_DELAY_MS);
}

void _wait_user_s(void)
{
	if (!sim.done && gtimer_wait(&sim.timer)) {
		return;
	}
	_sim_clear_response();
	fsm_gc_push_event(&sim_fsm, &success_e);
}

void _close_http_err_a(void)
{
	sim.http_error = true;
	_close_http_a();
}

void _close_http_a(void)
{
	sim.http_error = false;
	_simple_action_a("AT+HTTPTERM");
}

void _close_http_s(void)
{
	if (_sim_validate("ok")) {
		if (sim.http_error) {
			fsm_gc_push_event(&sim_fsm, &change_e);
		} else {
			fsm_gc_push_event(&sim_fsm, &success_e);
		}
	}
	if (gtimer_wait(&sim.timer)) {
		return;
	}
	fsm_gc_push_event(&sim_fsm, &error_e);
}

void _change_url_a(void)
{
#if SIM_MODULE_DEBUG
    printTagLog(SIM_TAG, "error - [%s]\n", strlen(sim.response) ? sim.response : "empty answer");
#endif
    if (sim.is_base_server) {
    	set_main_server();
#if SIM_MODULE_DEBUG
        printTagLog(SIM_TAG, "Change server url to: %s", sim.url);
#endif
    } else {
    	set_base_server();
#if SIM_MODULE_DEBUG
        printTagLog(SIM_TAG, "Change server url to: %s", sim.url);
#endif
    }
    _count_error_a();
}

void _count_error_a(void)
{
#if SIM_MODULE_DEBUG
    printTagLog(SIM_TAG, "error - [%s]\n", strlen(sim.response) ? sim.response : "empty answer");
#endif
	sim.errors++;
    sim.http_error = false;
}

void _count_error_s(void)
{
	if (sim.errors > SIM_MAX_ERRORS) {
		fsm_gc_push_event(&sim_fsm, &error_e);
	} else {
		fsm_gc_push_event(&sim_fsm, &success_e);
	}
}

void _reset_a(void)
{
#if SIM_MODULE_DEBUG
	printTagLog(SIM_TAG, "reset\n");
#endif
	memset(&sim, 0, sizeof(sim));
	sim.initialized = true;

	strncpy(sim.url, settings.url, sizeof(sim.url));
	fsm_gc_clear(&sim_fsm);

	HAL_GPIO_WritePin(SIM_MODULE_RESET_PORT, SIM_MODULE_RESET_PIN, GPIO_PIN_RESET);
	gtimer_start(&sim.timer, 1500);
}

void _reset_s(void)
{
	if (gtimer_wait(&sim.timer)) {
		return;
	}
	HAL_GPIO_WritePin(SIM_MODULE_RESET_PORT, SIM_MODULE_RESET_PIN, GPIO_PIN_SET);
	fsm_gc_push_event(&sim_fsm, &success_e);
}

void _after_reset_a(void)
{
	gtimer_start(&sim.timer, SIM_AFTER_RESET_MS);
}

void _after_reset_s(void)
{
	if (gtimer_wait(&sim.timer)) {
		return;
	}
	fsm_gc_push_event(&sim_fsm, &success_e);
}

