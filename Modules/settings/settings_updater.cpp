/* Copyright © 2024 Georgy E. All rights reserved. */

#include "settings.h"

#include <cstring>

#include "glog.h"
#include "soul.h"
#include "main.h"
#include "fsm_gc.h"

#include "Timer.h"
#include "SettingsDB.h"
#include "CodeStopwatch.h"


void _stng_check(void);

void _stng_init_s(void);
void _stng_idle_s(void);
void _stng_save_s(void);
void _stng_load_s(void);

void _stng_update_hash_a(void);


#if SETTINGS_BEDUG
const char STNGw_TAG[] = "STGw";
#endif


static unsigned old_hash = 0;


FSM_GC_CREATE(stng_fsm)

FSM_GC_CREATE_EVENT(stng_saved_e,   0)
FSM_GC_CREATE_EVENT(stng_updated_e, 0)

FSM_GC_CREATE_STATE(stng_init_s, _stng_init_s)
FSM_GC_CREATE_STATE(stng_idle_s, _stng_idle_s)
FSM_GC_CREATE_STATE(stng_save_s, _stng_save_s)
FSM_GC_CREATE_STATE(stng_load_s, _stng_load_s)

FSM_GC_CREATE_TABLE(
	stng_fsm_table,
	{&stng_init_s, &stng_updated_e, &stng_idle_s, _stng_update_hash_a},

	{&stng_idle_s, &stng_saved_e,   &stng_load_s, NULL},
	{&stng_idle_s, &stng_updated_e, &stng_save_s, NULL},

	{&stng_load_s, &stng_updated_e, &stng_idle_s, _stng_update_hash_a},
	{&stng_save_s, &stng_saved_e,   &stng_idle_s, _stng_update_hash_a}
)

extern "C" void settings_update()
{
#if SETTINGS_BEDUG
	utl::CodeStopwatch stopwatch("STNG", GENERAL_TIMEOUT_MS);
#endif
	if (!stng_fsm._initialized) {
		settings_reset(&settings);
		fsm_gc_init(&stng_fsm, stng_fsm_table, __arr_len(stng_fsm_table));
	}
	fsm_gc_proccess(&stng_fsm);
}

void _stng_check(void)
{
	reset_error(SETTINGS_LOAD_ERROR);
	if (!settings_check(&settings)) {
		set_error(SETTINGS_LOAD_ERROR);
#if SETTINGS_BEDUG
		printTagLog(STNGw_TAG, "settings check: not valid");
#endif
		settings_repair(&settings);
		set_status(NEED_SAVE_SETTINGS);
	}
}

void _stng_init_s(void)
{
	if (!is_status(MEMORY_INITIALIZED)) {
		return;
	}

	SettingsDB settingsDB(reinterpret_cast<uint8_t*>(&settings), settings_size());
	SettingsStatus status = settingsDB.load();
	if (status == SETTINGS_OK) {
		if (!settings_check(&settings)) {
			status = SETTINGS_ERROR;
		}
	}

	if (status != SETTINGS_OK) {
		settings_repair(&settings);
		status = settingsDB.save();
	}

	if (status == SETTINGS_OK) {
		reset_error(SETTINGS_LOAD_ERROR);
		settings_show();

		set_status(SETTINGS_INITIALIZED);
		set_status(SYSTEM_SOFTWARE_READY);

		_stng_check();
		fsm_gc_push_event(&stng_fsm, &stng_updated_e);
	} else {
		set_error(SETTINGS_LOAD_ERROR);
	}
}

void _stng_idle_s(void)
{
	if (is_status(NEED_SAVE_SETTINGS)) {
		reset_status(SYSTEM_SOFTWARE_READY);
		_stng_check();
		fsm_gc_push_event(&stng_fsm, &stng_updated_e);
	} else if (is_status(NEED_LOAD_SETTINGS)) {
		reset_status(SYSTEM_SOFTWARE_READY);
		_stng_check();
		fsm_gc_push_event(&stng_fsm, &stng_saved_e);
	}
}

void _stng_save_s(void)
{
	SettingsStatus status = SETTINGS_OK;
	SettingsDB settingsDB(reinterpret_cast<uint8_t*>(&settings), settings_size());
	if (old_hash != util_hash((uint8_t*)&settings, sizeof(settings))) {
		status = settingsDB.save();
	}
	if (status == SETTINGS_OK) {
		_stng_check();
		fsm_gc_push_event(&stng_fsm, &stng_saved_e);

		settings_show();

		reset_error(SETTINGS_LOAD_ERROR);

		reset_status(NEED_SAVE_SETTINGS);
		set_status(SYSTEM_SOFTWARE_READY);
	}
}

void _stng_load_s(void)
{
	SettingsDB settingsDB(reinterpret_cast<uint8_t*>(&settings), settings_size());
	SettingsStatus status = settingsDB.load();
	if (status == SETTINGS_OK) {
		_stng_check();
		fsm_gc_push_event(&stng_fsm, &stng_updated_e);

		settings_show();

		reset_error(SETTINGS_LOAD_ERROR);
		reset_status(NEED_LOAD_SETTINGS);
		set_status(SYSTEM_SOFTWARE_READY);
	}
}

void _stng_update_hash_a(void)
{
	old_hash = util_hash((uint8_t*)&settings, sizeof(settings));
}
