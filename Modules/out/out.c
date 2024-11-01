/* Copyright © 2024 Georgy E. All rights reserved. */

#include "out.h"

#include "main.h"
#include "soul.h"
#include "gutils.h"
#include "settings.h"


static const util_port_pin_t outs[SETTINGS_OUTPUTS_CNT] = {
	{OUT_A_GPIO_Port, OUT_A_Pin},
	{OUT_B_GPIO_Port, OUT_B_Pin},
	{OUT_C_GPIO_Port, OUT_C_Pin},
	{OUT_D_GPIO_Port, OUT_D_Pin},
};


void out_tick()
{
	if (has_errors() || is_status(LOADING) || !is_status(WORKING)) {
		for (unsigned i = 0; i < __arr_len(outs); i++) {
			HAL_GPIO_WritePin(outs[i].port, outs[i].pin, GPIO_PIN_RESET);
		}
		return;
	}

	for (unsigned i = 0; i < __arr_len(outs); i++) {
		HAL_GPIO_WritePin(outs[i].port, outs[i].pin, settings.outputs[i]);
	}
}
