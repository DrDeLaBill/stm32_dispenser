/* Copyright © 2024 Georgy E. All rights reserved. */

#ifndef _SYSTEM_H_
#define _SYSTEM_H_


#ifdef __cplusplus
extern "C" {
#endif


#include <stdint.h>

#include "soul.h"
#include "system_config.h"


#ifdef DEBUG
#   define SYSTEM_BEDUG (1)
#endif

#define SYSTEM_CANARY_WORD ((uint32_t)0xBEDAC0DE)

#ifndef SYSTEM_ADC_VOLTAGE_COUNT
#   define SYSTEM_ADC_VOLTAGE_COUNT (1)
#endif

void system_pre_load(void);
void system_post_load(void);

void system_tick();

bool is_system_ready();

void system_error_handler(SOUL_STATUS error);

uint32_t get_system_power(void);

void system_reset_i2c_errata(void);

char* get_system_serial_str(void);

void system_hsi_config(void);
void system_hse_config(void);
void system_error_loop(void);

void system_sys_tick_reanimation(void);

uint16_t get_system_adc(unsigned index);

bool get_system_rtc_ram(const uint8_t idx, uint8_t* data);
bool set_system_rtc_ram(const uint8_t idx, const uint8_t data);


#ifdef __cplusplus
}
#endif


#endif
