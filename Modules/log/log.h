/* Copyright © 2023 Georgy E. All rights reserved. */

#ifndef _LOG_H_
#define _LOG_H_


#define LOG_WORK_BEDUG  (0)
#define LOG_PARSE_BEDUG (0)


void log_init();
void log_tick();
extern "C" void log_reset_timers();


#endif
