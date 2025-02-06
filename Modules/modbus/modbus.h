/* Copyright © 2025 Georgy E. All rights reserved. */

#ifndef _MODBUS_H_
#define _MODBUS_H_


#ifdef __cplusplus
extern "C" {
#endif


#include <stdint.h>


void modbus_tick();
void modbus_input();
uint32_t get_press();


#ifdef __cplusplus
}
#endif


#endif
