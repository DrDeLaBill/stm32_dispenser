/* Copyright © 2023 Georgy E. All rights reserved. */

#ifndef INC_PRESSURE_SENSOR_H_
#define INC_PRESSURE_SENSOR_H_


#ifdef __cplusplus
extern "C" {
#endif


#include "stm32f1xx_hal.h"

#include <stdbool.h>
#include <stdint.h>

#include "gutils.h"


void tempr_tick();
uint16_t get_tempr();


#ifdef __cplusplus
}
#endif


#endif /* INC_PRESSURE_SENSOR_H_ */
