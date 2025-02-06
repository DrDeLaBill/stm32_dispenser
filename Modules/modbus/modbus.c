/* Copyright © 2025 Georgy E. All rights reserved. */

#include "modbus.h"

#include "main.h"
#include "glog.h"
#include "gutils.h"
#include "fsm_gc.h"
#include "gsystem.h"
#include "hal_defs.h"
#include "modbus_rtu_master.h"


#define MB_DELAY_MS     (SECOND_MS)
#define MB_TIMEOUT_MS   (SECOND_MS)
#define MB_BAUDRATE_REG (0)
#define MB_UNIT_REG     (102)
#define MB_VALUE_REG    (122)
#define MB_UNIT_REG_MPA (2)


static void _uart_setup();
static void _request_data_sender(uint8_t* data, uint32_t len);
static void _response_packet_handler(modbus_response_t* packet);
static void _master_internal_error_handler(void);


typedef struct _mbs_baudrate_t {
	uint16_t num;
	uint32_t baudrate;
	uint32_t Parity;
} mbs_baudrate_t;

typedef struct _modbus_t {
	bool           initialized;
	bool           received;
	gtimer_t       timer;
	unsigned       counter;
	uint8_t        slave_id;
	uint32_t       value;
	mbs_baudrate_t baudrates[9];
	uint8_t        response[2 * sizeof(uint16_t)];
	uint8_t        input_byte;
} modbus_t;

static const char TAG[] = "MBS";
static modbus_t mb = {
	.initialized = false,
	.received    = false,
	.timer       = {0},
	.counter     = 0,
	.slave_id    = 0x01,
	.value       = 0,
	.baudrates   = {
		{3,  1050, UART_PARITY_NONE}, // baudrate: 1050
		{4,  1050, UART_PARITY_NONE},
		{5,  1050, UART_PARITY_NONE},
		{6,  1050, UART_PARITY_ODD},
		{7,  1050, UART_PARITY_ODD},
		{8,  1050, UART_PARITY_ODD},
		{9,  1050, UART_PARITY_EVEN},
		{10, 1050, UART_PARITY_EVEN},
		{11, 1050, UART_PARITY_EVEN},
//		{3,  9600,  UART_PARITY_NONE}, // baudrate: 1050
//		{4,  19200, UART_PARITY_NONE},
//		{5,  57600, UART_PARITY_NONE},
//		{6,  9600,  UART_PARITY_ODD},
//		{7,  19200, UART_PARITY_ODD},
//		{8,  57600, UART_PARITY_ODD},
//		{9,  9600,  UART_PARITY_EVEN},
//		{10, 19200, UART_PARITY_EVEN},
//		{11, 57600, UART_PARITY_EVEN},
	},
	.response     = {0},
	.input_byte   = 0,
};

FSM_GC_CREATE(mbs_fsm)

FSM_GC_CREATE_EVENT(success_e, 0)
FSM_GC_CREATE_EVENT(next_e,    0)
FSM_GC_CREATE_EVENT(timeout_e, 1)

FSM_GC_CREATE_STATE(init_s,   _init_s)
FSM_GC_CREATE_STATE(search_s, _search_s)
FSM_GC_CREATE_STATE(setup_s,  _setup_s)
FSM_GC_CREATE_STATE(read_s,   _read_s)

FSM_GC_CREATE_ACTION(search_a,         _search_a)
FSM_GC_CREATE_ACTION(search_iterate_a, _search_iterate_a)
FSM_GC_CREATE_ACTION(setup_a,          _setup_a)
FSM_GC_CREATE_ACTION(read_a,           _read_a)
FSM_GC_CREATE_ACTION(reset_a,          _reset_a)

FSM_GC_CREATE_TABLE(
	mbs_fsm_table,
	{&init_s,   &success_e, &search_s, &search_a},

	{&search_s, &success_e, &setup_s,  &setup_a},
	{&search_s, &next_e,    &search_s, &search_iterate_a},
	{&search_s, &timeout_e, &search_s, &search_iterate_a},

	{&setup_s,  &success_e, &read_s,   &read_a},
	{&setup_s,  &timeout_e, &search_s, &reset_a},

	{&read_s,   &success_e, &read_s,   &read_a},
	{&read_s,   &timeout_e, &search_s, &reset_a},
)


void modbus_tick()
{
	if (!mb.initialized) {
		fsm_gc_init(&mbs_fsm, mbs_fsm_table, __arr_len(mbs_fsm_table));
	    mb.initialized = true;
	}
	fsm_gc_process(&mbs_fsm);
}

void modbus_input()
{
	modbus_master_recieve_data_byte(mb.input_byte);
}

uint32_t get_press()
{
	return mb.value;
}

void _request_data_sender(uint8_t* data, uint32_t len)
{
	mb.received = false;
	if (HAL_UART_Transmit(&RS485_UART, data, (uint16_t)len, 100) != HAL_OK) {
    	printTagLog(TAG, "modbus error send");
    	fsm_gc_push_event(&mbs_fsm, &timeout_e);
	}
}

void _response_packet_handler(modbus_response_t* packet)
{
	mb.received = false;
    if (packet->status != MODBUS_NO_ERROR) {
    	printTagLog(TAG, "modbus error: %u", packet->status);
    	fsm_gc_push_event(&mbs_fsm, &timeout_e);
    } else {
    	mb.received = true;
    	memcpy(mb.response, packet->response ,sizeof(mb.response));
		printTagLog(TAG, "modbus response: %02X %02X", mb.response[0], mb.response[1]);
    }
	gtimer_start(&mb.timer, MB_DELAY_MS);
}

void _master_internal_error_handler(void)
{
	mb.received = false;
	printTagLog(TAG, "modbus error");
	fsm_gc_push_event(&mbs_fsm, &timeout_e);
}

void _uart_setup()
{
	UART_HandleTypeDef huart = {0};
	huart.Instance          = RS485_UART.Instance;
	huart.Init.BaudRate     = mb.baudrates[mb.counter].baudrate;
	huart.Init.WordLength   = UART_WORDLENGTH_8B;
	huart.Init.StopBits     = UART_STOPBITS_1;
	huart.Init.Parity       = mb.baudrates[mb.counter].Parity;
	huart.Init.Mode         = UART_MODE_TX_RX;
	huart.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
	huart.Init.OverSampling = UART_OVERSAMPLING_16;
	HAL_UART_AbortReceive(&RS485_UART);
	if (HAL_UART_DeInit(&RS485_UART) != HAL_OK) {
		system_error_handler((SOUL_STATUS)MODBUS_ERROR);
	}
	if (HAL_UART_Init(&huart) == HAL_OK) {
		memcpy((void*)&RS485_UART, (void*)&huart, sizeof(huart));
	} else {
		system_error_handler((SOUL_STATUS)MODBUS_ERROR);
	}
	HAL_UART_Receive_IT(&RS485_UART, &mb.input_byte, sizeof(char));
	modbus_master_read_holding_registers(mb.slave_id, MB_BAUDRATE_REG, 1);
	gtimer_start(&mb.timer, MB_TIMEOUT_MS);
}

void _init_s(void)
{
    modbus_master_set_request_data_sender(&_request_data_sender);
    modbus_master_set_response_packet_handler(&_response_packet_handler);
    modbus_master_set_internal_error_handler(&_master_internal_error_handler);
    fsm_gc_push_event(&mbs_fsm, &success_e);
}

void _search_a(void)
{
	mb.counter = 0;
	_uart_setup();
}

void _search_iterate_a(void)
{
	if (mb.counter + 1 < __arr_len(mb.baudrates)) {
		mb.counter++;
	} else {
		mb.slave_id = mb.slave_id < 0x0F ? mb.slave_id + 1 : 0;
		printTagLog(TAG, "Search new slave ID: 0x%02X", mb.slave_id);
		mb.counter = 0;
	}
	_uart_setup();
}

void _search_s(void)
{
	if (mb.received && mb.response[0] == mb.baudrates[mb.counter].num) {
		fsm_gc_push_event(&mbs_fsm, &success_e);
	}
	if (gtimer_wait(&mb.timer)) {
		return;
	}
	fsm_gc_push_event(&mbs_fsm, &next_e);
}

void _setup_a(void)
{
	modbus_master_preset_single_register(mb.slave_id, MB_UNIT_REG, MB_UNIT_REG_MPA);
	gtimer_start(&mb.timer, MB_TIMEOUT_MS);
}

void _setup_s(void)
{
	if (mb.received) {
		fsm_gc_push_event(&mbs_fsm, &success_e);
	}
	if (gtimer_wait(&mb.timer)) {
		return;
	}
	fsm_gc_push_event(&mbs_fsm, &timeout_e);
}

void _read_a(void)
{
	modbus_master_read_holding_registers(mb.slave_id, MB_VALUE_REG, 2);
	gtimer_start(&mb.timer, MB_DELAY_MS);
}

void _read_s(void)
{
	if (mb.received) {
		mb.value =
			((uint32_t)mb.response[3] << (3 * BITS_IN_BYTE)) +
			((uint32_t)mb.response[2] << (2 * BITS_IN_BYTE)) +
			((uint32_t)mb.response[1] << (1 * BITS_IN_BYTE)) +
			(uint32_t)mb.response[0];
		fsm_gc_push_event(&mbs_fsm, &success_e);
	}
	if (gtimer_wait(&mb.timer)) {
		return;
	}
	fsm_gc_push_event(&mbs_fsm, &timeout_e);
}

void _reset_a(void)
{
	mb.value = 0;
	mb.counter = 0;
	fsm_gc_clear(&mbs_fsm);
	modbus_master_timeout();
}
