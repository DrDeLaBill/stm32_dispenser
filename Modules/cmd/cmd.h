/* Copyright © 2024 Georgy E. All rights reserved. */

#ifndef _CMD_H_
#define _CMD_H_


#ifdef __cplusplus
extern "C" {
#endif


#include <stdint.h>


void cmd_input(uint8_t byte);
void cmd_process();


#ifdef __cplusplus
}
#endif


#endif
