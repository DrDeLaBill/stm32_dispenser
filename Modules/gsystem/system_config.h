/* Copyright © 2024 Georgy E. All rights reserved. */

#ifndef _SYSTEM_CONFIG_H_
#define _SYSTEM_CONFIG_H_


#include "soul.h"


#ifdef __cplusplus
extern "C" {
#endif


typedef enum _CUSTOM_SOUL_STATUSES {
	HAS_NEW_RECORD           = RESERVED_STATUS_01,
	NEW_RECORD_WAS_NOT_SAVED = RESERVED_STATUS_02
} CUSTOM_SOUL_STATUSES;


#ifdef __cplusplus
}
#endif


#endif
