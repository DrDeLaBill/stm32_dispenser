/* Copyright © 2024 Georgy E. All rights reserved. */

#include "system.h"

#include <stdbool.h>

#include "main.h"
#include "glog.h"
#include "clock.h"
#include "hal_defs.h"

#if defined(SYSTEM_DS1307_CLOCK)
#   include "ds1307.h"
#endif

#define SYSTEM_BKUP_STATUS_TYPE uint32_t
#if defined(SYSTEM_DS1307_CLOCK)
#   define SYSTEM_BKUP_SIZE (DS1307_REG_RAM_END - DS1307_REG_RAM - sizeof(SYSTEM_BKUP_STATUS_TYPE))
#else
#   define SYSTEM_BKUP_SIZE (RTC_BKP_NUMBER - RTC_BKP_DR2 - sizeof(SYSTEM_BKUP_STATUS_TYPE))
#endif


static void _system_start_ram_fill(void);
static void _system_error_timer_start(uint32_t delay_ms);
static bool _system_error_timer_wait(void);
static void _system_error_timer_disable(void);


static const char SYSTEM_TAG[] = "SYS";
static const uint32_t err_delay_ms = 30 * MINUTE_MS;

static bool system_hsi_initialized = false;
static util_old_timer_t err_timer = {0};

uint16_t SYSTEM_ADC_VOLTAGE[SYSTEM_ADC_VOLTAGE_COUNT] = {0};


typedef enum _watchdog_type_t {
	HARDWARE_WATCHDOG,
	SOFTWARE_WATCHDOG
} watchdog_type_t;

typedef struct _watchdogs_t {
	void             (*action)(void);
	uint32_t         delay_ms;
	util_old_timer_t timer;
	watchdog_type_t  type;
} watchdogs_t;


extern void power_watchdog_check();
extern void restart_watchdog_check();
extern void sys_clock_watchdog_check();
extern void ram_watchdog_check();
extern void rtc_watchdog_check();
extern void memory_watchdog_check();
extern void adc_watchdog_check();
watchdogs_t watchdogs[] = {
	{restart_watchdog_check,   SECOND_MS / 10, {0,0}, HARDWARE_WATCHDOG},
	{sys_clock_watchdog_check, SECOND_MS / 10, {0,0}, HARDWARE_WATCHDOG},
	{ram_watchdog_check,       5 * SECOND_MS,  {0,0}, HARDWARE_WATCHDOG},
	{adc_watchdog_check,       SECOND_MS / 10, {0,0}, HARDWARE_WATCHDOG},
	{power_watchdog_check,     SECOND_MS,      {0,0}, SOFTWARE_WATCHDOG},
	{rtc_watchdog_check,       SECOND_MS,      {0,0}, SOFTWARE_WATCHDOG},
	{memory_watchdog_check,    SECOND_MS,      {0,0}, SOFTWARE_WATCHDOG},
};

void system_pre_load(void)
{
	_system_start_ram_fill();

	if (!MCUcheck()) {
		set_error(MCU_ERROR);
		system_error_handler(get_first_error());
		while(1) {}
	}

	RCC->CR |= RCC_CR_HSEON;

	_system_error_timer_start(SECOND_MS);
	while (_system_error_timer_wait()) {
		if (RCC->CR & RCC_CR_HSERDY) {
			break;
		}
	}
	if (!(RCC->CR & RCC_CR_HSERDY)) {
		set_status(SYS_TICK_ERROR);
		set_error(SYS_TICK_FAULT);
	}
	_system_error_timer_disable();

	util_old_timer_start(&err_timer, err_delay_ms);

	set_status(SYSTEM_HARDWARE_STARTED);
}

void system_post_load(void)
{
#if SYSTEM_BEDUG
	printTagLog(SYSTEM_TAG, "System postload");
#endif

	set_status(SYSTEM_SOFTWARE_STARTED);

	SystemInfo();

	const uint32_t delay_ms = 10000;
	util_old_timer_t timer = {0};
	bool need_error_timer = is_status(SYS_TICK_FAULT);
	if (need_error_timer) {
		_system_error_timer_start(delay_ms);
	} else {
		util_old_timer_start(&timer, delay_ms);
	}
	while (1) {
		adc_watchdog_check();

		uint32_t voltage = get_system_power();
		if (STM_MIN_VOLTAGEx10 <= voltage && voltage <= STM_MAX_VOLTAGEx10) {
			break;
		}

		if (is_status(SYS_TICK_FAULT) && !_system_error_timer_wait()) {
			set_error(SYS_TICK_ERROR);
			break;
		} else if (!util_old_timer_wait(&timer)) {
			set_error(SYS_TICK_ERROR);
			break;
		}
	}
	if (need_error_timer) {
		_system_error_timer_disable();
	}

	if (is_error(SYS_TICK_ERROR) || is_error(POWER_ERROR)) {
		system_error_handler(
			(get_first_error() == INTERNAL_ERROR) ?
				LOAD_ERROR :
				(SOUL_STATUS)get_first_error()
		);
	}

	clock_begin();
	SYSTEM_BKUP_STATUS_TYPE status = 0;
	for (uint8_t i = 0; i < sizeof(status); i++) {
		uint8_t data = 0;
		if (!get_clock_ram(i, &data)) {
			status = 0;
			break;
		}
		((uint8_t*)&status)[i] = data;
	}
	set_last_error((SOUL_STATUS)status);
	set_clock_ram(0, 0);
#if SYSTEM_BEDUG
	if (get_last_error()) {
		printTagLog(SYSTEM_TAG, "Last reload error: %s", get_status_name(get_last_error()));
	}
#endif

#if SYSTEM_BEDUG
	printTagLog(SYSTEM_TAG, "System loaded");
#endif
}

void system_tick()
{
#ifdef DEBUG
	static unsigned kFLOPScounter = 0;
	static util_old_timer_t kFLOPSTimer = {0,(10 * SECOND_MS)};
#endif
	static util_old_timer_t timer = {0,0};
	static unsigned index = 0;

	if (!util_old_timer_wait(&err_timer)) {
		system_error_handler(
			get_first_error() ? get_first_error() : INTERNAL_ERROR
		);
	}
	if (!has_errors()) {
		util_old_timer_start(&err_timer, err_delay_ms);
	}

#ifdef DEBUG
	kFLOPScounter++;
	if (!util_old_timer_wait(&kFLOPSTimer)) {
		printTagLog(
			SYSTEM_TAG,
			"kFLOPS: %lu.%lu",
			kFLOPScounter / (10 * SECOND_MS),
			(kFLOPScounter / SECOND_MS) % 10
		);
		kFLOPScounter = 0;
		util_old_timer_start(&kFLOPSTimer, (10 * SECOND_MS));
	}
	if (has_new_status_data()) {
		show_statuses();
	}

#endif

#if defined(DEBUG) || defined(GBEDUG_FORCE)
	if (has_new_error_data()) {
		show_errors();
	}
#endif

	if (!is_error(STACK_ERROR) &&
		!is_error(SYS_TICK_ERROR)
	) {
		set_status(SYSTEM_HARDWARE_READY);
	} else {
		reset_status(SYSTEM_HARDWARE_READY);
	}

	if (!is_status(SYS_TICK_FAULT) && util_old_timer_wait(&timer)) {
		return;
	}
	util_old_timer_start(&timer, 50);

	if (!is_status(SYSTEM_HARDWARE_READY)) {
		reset_status(SYSTEM_SOFTWARE_READY);
	}

	if (index >= __arr_len(watchdogs)) {
		index = 0;
	}

	if (!is_status(SYSTEM_HARDWARE_READY) &&
		watchdogs[index].type == SOFTWARE_WATCHDOG
	) {
		index = 0;
	}

	if (is_status(SYS_TICK_FAULT) || !util_old_timer_wait(&watchdogs[index].timer)) {
		util_old_timer_start(&watchdogs[index].timer, watchdogs[index].delay_ms);
		watchdogs[index].action();
	}

	index++;
}

bool is_system_ready()
{
	return !(has_errors() || is_status(SYSTEM_SAFETY_MODE) || !is_status(SYSTEM_HARDWARE_READY) || !is_status(SYSTEM_SOFTWARE_READY));
}

void system_error_handler(SOUL_STATUS error)
{
	static bool called = false;
	if (called) {
		return;
	}
	called = true;

	set_error(error);

	if (!has_errors()) {
		error = INTERNAL_ERROR;
	}

#if SYSTEM_BEDUG
	printTagLog(SYSTEM_TAG, "system_error_handler called error=%s", get_status_name(error));
#endif

	if (is_error(SYS_TICK_ERROR) && !system_hsi_initialized) {
		system_hsi_config();
	}

	if (!is_clock_started()) {
#if defined(SYSTEM_DS1307_CLOCK)
		SYSTEM_CLOCK_I2C.Instance = I2C1;
		SYSTEM_CLOCK_I2C.Init.ClockSpeed = 100000;
		SYSTEM_CLOCK_I2C.Init.DutyCycle = I2C_DUTYCYCLE_2;
		SYSTEM_CLOCK_I2C.Init.OwnAddress1 = 0;
		SYSTEM_CLOCK_I2C.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
		SYSTEM_CLOCK_I2C.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
		SYSTEM_CLOCK_I2C.Init.OwnAddress2 = 0;
		SYSTEM_CLOCK_I2C.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
		SYSTEM_CLOCK_I2C.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
		if (HAL_I2C_Init(&SYSTEM_CLOCK_I2C) != HAL_OK) {
			set_error(I2C_ERROR);
		}
#endif
		clock_begin();
	}
	if (!is_clock_ready()) {
		set_clock_ready();
	}

	if (is_clock_ready()) {
		set_clock_ram(0, (uint32_t)error);
	}

	const uint32_t delay_ms = 30 * SECOND_MS;
	util_old_timer_t timer = {0};
	bool need_error_timer = is_status(SYS_TICK_FAULT);
	if (need_error_timer) {
		_system_error_timer_start(delay_ms);
	} else {
		util_old_timer_start(&timer, delay_ms);
	}
	while(1) {
		system_error_loop();

		system_tick();

		if (is_status(SYS_TICK_FAULT) && !_system_error_timer_wait()) {
			break;
		} else if (!util_old_timer_wait(&timer)) {
			break;
		}
	}
	if (need_error_timer) {
		_system_error_timer_disable();
	}

#if SYSTEM_BEDUG
	_system_error_timer_start(100);
	printTagLog(SYSTEM_TAG, "system reset");
	while(_system_error_timer_wait());
	_system_error_timer_disable();
#endif

	NVIC_SystemReset();
}

uint32_t get_system_power(void)
{
	if (!SYSTEM_ADC_VOLTAGE[0]) {
		return 0;
	}
	return (STM_ADC_MAX * STM_REF_VOLTAGEx10) / SYSTEM_ADC_VOLTAGE[0];
}


__attribute__((weak)) void system_hse_config(void)
{
	RCC_OscInitTypeDef RCC_OscInitStruct   = {0};
	RCC_ClkInitTypeDef RCC_ClkInitStruct   = {0};
	RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSI|RCC_OSCILLATORTYPE_HSE;
	RCC_OscInitStruct.HSEState = RCC_HSE_ON;
	RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
	RCC_OscInitStruct.HSIState = RCC_HSI_ON;
	RCC_OscInitStruct.LSIState = RCC_LSI_ON;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
	RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
		set_error(SYS_TICK_FAULT);
		return;
	}

	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
						  |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) {
		set_error(SYS_TICK_FAULT);
		return;
	}
	PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_RTC|RCC_PERIPHCLK_ADC;
	PeriphClkInit.RTCClockSelection = RCC_RTCCLKSOURCE_LSI;
	PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV6;
	if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK) {
		set_error(SYS_TICK_FAULT);
		return;
	}

	HAL_RCC_EnableCSS();
}

__attribute__((weak)) void system_hsi_config(void)
{
#ifdef STM32F1
	RCC_OscInitTypeDef RCC_OscInitStruct = {0};
	RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
	RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_LSI;
	RCC_OscInitStruct.HSIState = RCC_HSI_ON;
	RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
	RCC_OscInitStruct.LSIState = RCC_LSI_ON;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI_DIV2;
	RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
		set_error(SYS_TICK_ERROR);
		return;
	}

	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
							  |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK) {
		set_error(SYS_TICK_ERROR);
		return;
	}
	PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_RTC|RCC_PERIPHCLK_ADC;
	PeriphClkInit.RTCClockSelection = RCC_RTCCLKSOURCE_LSI;
	PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV6;
	if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK) {
		set_error(SYS_TICK_ERROR);
		return;
	}
#elif defined(STM32F4)
	RCC_OscInitTypeDef RCC_OscInitStruct = {0};
	RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

	/** Configure the main internal regulator output voltage
	*/
	__HAL_RCC_PWR_CLK_ENABLE();
	__HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

	/** Initializes the RCC Oscillators according to the specified parameters
	* in the RCC_OscInitTypeDef structure.
	*/
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_LSI
										|RCC_OSCILLATORTYPE_LSE;
	RCC_OscInitStruct.LSEState = RCC_LSE_ON;
	RCC_OscInitStruct.HSIState = RCC_HSI_ON;
	RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
	RCC_OscInitStruct.LSIState = RCC_LSI_ON;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
	RCC_OscInitStruct.PLL.PLLM = 8;
	RCC_OscInitStruct.PLL.PLLN = 84;
	RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
	RCC_OscInitStruct.PLL.PLLQ = 4;
	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
		set_error(SYS_TICK_ERROR);
		return;
	}

	/** Initializes the CPU, AHB and APB buses clocks
	*/
	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
									|RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) {
		set_error(SYS_TICK_ERROR);
		return;
	}
#else
#   error "Please select your controller"
#endif

	system_hsi_initialized = true;
}

__attribute__((weak)) void system_error_loop(void) {}

void system_reset_i2c_errata(void)
{
#if SYSTEM_BEDUG
	printTagLog(SYSTEM_TAG, "RESET I2C (ERRATA)");
#endif

	if (!SYSTEM_I2C.Instance) {
		return;
	}

	HAL_I2C_DeInit(&SYSTEM_I2C);

	GPIO_TypeDef* I2C_PORT = GPIOB;
	uint16_t I2C_SDA_Pin = GPIO_PIN_7;
	uint16_t I2C_SCL_Pin = GPIO_PIN_6;

	GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.Pin   = I2C_SCL_Pin | I2C_SCL_Pin;
	GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_OD;
	GPIO_InitStruct.Pull  = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(I2C_PORT, &GPIO_InitStruct);

	SYSTEM_I2C.Instance->CR1 &= (unsigned)~(0x0001);

	GPIO_InitTypeDef GPIO_InitStructure = {0};
	GPIO_InitStructure.Mode = GPIO_MODE_OUTPUT_OD;
#ifdef STM32F4
	GPIO_InitStructure.Alternate = 0;
#endif
	GPIO_InitStructure.Pull = GPIO_PULLUP;
	GPIO_InitStructure.Speed = GPIO_SPEED_FREQ_HIGH;


	GPIO_InitStructure.Pin = I2C_SCL_Pin;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStructure);
	GPIO_InitStructure.Pin = I2C_SDA_Pin;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStructure);

	typedef struct _reseter_t {
		uint16_t      pin;
		GPIO_PinState stat;
	} reseter_t;
	const uint32_t TIMEOUT_MS = 2000;
	util_old_timer_t timer = {0};
	reseter_t reseter[] = {
		{I2C_SCL_Pin, GPIO_PIN_SET},
		{I2C_SDA_Pin, GPIO_PIN_SET},
		{I2C_SCL_Pin, GPIO_PIN_RESET},
		{I2C_SDA_Pin, GPIO_PIN_RESET},
		{I2C_SCL_Pin, GPIO_PIN_SET},
		{I2C_SDA_Pin, GPIO_PIN_SET},
	};

	for (unsigned i = 0; i < __arr_len(reseter); i++) {
		HAL_GPIO_WritePin(I2C_PORT, reseter[i].pin, reseter[i].stat);
		util_old_timer_start(&timer, TIMEOUT_MS);
		while(reseter[i].stat != HAL_GPIO_ReadPin(I2C_PORT, reseter[i].pin)) {
			if (!util_old_timer_wait(&timer)) {
				system_error_handler(I2C_ERROR);
			}
			asm("nop");
		}
	}

	GPIO_InitStructure.Mode = GPIO_MODE_AF_OD;
#ifdef STM32F4
	GPIO_InitStructure.Alternate = GPIO_AF4_I2C1;
#endif

	GPIO_InitStructure.Pin = I2C_SCL_Pin;
	HAL_GPIO_Init(I2C_PORT, &GPIO_InitStructure);

	GPIO_InitStructure.Pin = I2C_SDA_Pin;
	HAL_GPIO_Init(I2C_PORT, &GPIO_InitStructure);

	SYSTEM_I2C.Instance->CR1 |= 0x8000;
	asm("nop");
	SYSTEM_I2C.Instance->CR1 &= (unsigned)~0x8000;
	asm("nop");

	SYSTEM_I2C.Instance->CR1 |= 0x0001;

	HAL_I2C_Init(&SYSTEM_I2C);
}

char* get_system_serial_str(void)
{
	uint32_t uid_base = 0x1FFFF7E8;

	uint16_t *idBase0 = (uint16_t*)(uid_base);
	uint16_t *idBase1 = (uint16_t*)(uid_base + 0x02);
	uint32_t *idBase2 = (uint32_t*)(uid_base + 0x04);
	uint32_t *idBase3 = (uint32_t*)(uid_base + 0x08);

	static char str_uid[25] = {0};
	memset((void*)str_uid, 0, sizeof(str_uid));
	sprintf(str_uid, "%04X%04X%08lX%08lX", *idBase0, *idBase1, *idBase2, *idBase3);

	return str_uid;
}

void system_sys_tick_reanimation(void)
{
	extern void SystemClock_Config(void);

	__disable_irq();

	set_error(SYS_TICK_FAULT);
	set_error(SYS_TICK_ERROR);

	RCC->CIR |= RCC_CIR_CSSC;

	_system_error_timer_start(SECOND_MS);
	while (_system_error_timer_wait());
	_system_error_timer_disable();

	RCC->CR |= RCC_CR_HSEON;
	_system_error_timer_start(SECOND_MS);
	while (_system_error_timer_wait()) {
		if (RCC->CR & RCC_CR_HSERDY) {
			reset_error(SYS_TICK_FAULT);
			reset_error(SYS_TICK_ERROR);
			break;
		}
	}
	_system_error_timer_disable();


	if (is_error(SYS_TICK_ERROR)) {
		system_hsi_config();
		reset_error(SYS_TICK_ERROR);
	} else {
		system_hse_config();
	}
	reset_error(NON_MASKABLE_INTERRUPT);


#if defined(DEBUG) || defined(GBEDUG_FORCE)
	if (is_status(SYS_TICK_FAULT)) {
		printTagLog(SYSTEM_TAG, "Critical external RCC failure");
		printTagLog(SYSTEM_TAG, "The internal RCC has been started");
	} else {
		printTagLog(SYSTEM_TAG, "Critical external RCC failure");
		printTagLog(SYSTEM_TAG, "The external RCC has been restarted");
	}
#endif

	__enable_irq();
}

uint16_t get_system_adc(unsigned index)
{
#if SYSTEM_ADC_VOLTAGE_COUNT <= 1
	return 0;
#else
	if (index + 1 >= SYSTEM_ADC_VOLTAGE_COUNT) {
		return 0;
	}
	return SYSTEM_ADC_VOLTAGE[index+1];
#endif
}

bool get_system_rtc_ram(const uint8_t idx, uint8_t* data)
{
	if (idx + sizeof(SYSTEM_BKUP_STATUS_TYPE) >= SYSTEM_BKUP_SIZE) {
		return false;
	}
	return get_clock_ram(idx + sizeof(SYSTEM_BKUP_STATUS_TYPE), data);
}

bool set_system_rtc_ram(const uint8_t idx, const uint8_t data)
{
	if (idx + sizeof(SYSTEM_BKUP_STATUS_TYPE) >= SYSTEM_BKUP_SIZE) {
		return false;
	}
	return set_clock_ram(idx + sizeof(SYSTEM_BKUP_STATUS_TYPE), data);
}

void _system_start_ram_fill(void)
{
	extern unsigned _ebss;
	volatile unsigned *top, *start;
	__asm__ volatile ("mov %[top], sp" : [top] "=r" (top) : : );
	start = &_ebss;
	start++;
	while (start < top) {
		*(start++) = SYSTEM_CANARY_WORD;
	}
}

typedef struct _error_timer_t {
	TIM_TypeDef tim;
	bool        enabled;
	uint32_t    delay_ms;
} error_timer_t;

static error_timer_t error_timer = {0};

void _system_error_timer_start(uint32_t delay_ms)
{
	memset(&error_timer, 0, sizeof(error_timer));
	error_timer.enabled = READ_BIT(RCC->APB1ENR, RCC_APB1ENR_TIM4EN);
	if (error_timer.enabled) {
		memcpy(&error_timer.tim, TIM1, sizeof(error_timer.tim));
	}

	__TIM1_CLK_ENABLE();
	unsigned count_multiplier = 10;
	unsigned count_cnt = 1 * count_multiplier * delay_ms;
	unsigned presc = HAL_RCC_GetSysClockFreq() / (count_multiplier * delay_ms);
	TIM1->SR = 0;
	TIM1->PSC = presc - 1;
	TIM1->ARR = count_cnt - 1;
	TIM1->CNT = 0;
	TIM1->CR1 &= ~(TIM_CR1_DIR);

	TIM1->CR1 |= TIM_CR1_CEN;
}

bool _system_error_timer_wait(void)
{
	return !(TIM1->SR & TIM_SR_CC1IF);
}

void _system_error_timer_disable(void)
{
	TIM1->SR &= ~(TIM_SR_UIF | TIM_SR_CC1IF);
	TIM1->CNT = 0;

	if (error_timer.enabled) {
		memcpy(TIM1, &error_timer.tim, sizeof(error_timer.tim));
	} else {
		TIM1->CR1 &= ~(TIM_CR1_CEN);
		__TIM1_CLK_DISABLE();
	}
}
