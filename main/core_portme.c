/*
Copyright 2018 Embedded Microprocessor Benchmark Consortium (EEMBC)

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

Original Author: Shay Gal-on
*/

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include "coremark.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define ITERATIONS 	2500 // Reduced iterations for testing

#if VALIDATION_RUN
	volatile ee_s32 seed1_volatile=0x3415;
	volatile ee_s32 seed2_volatile=0x3415;
	volatile ee_s32 seed3_volatile=0x66;
#endif
#if PERFORMANCE_RUN
	volatile ee_s32 seed1_volatile=0x0;
	volatile ee_s32 seed2_volatile=0x0;
	volatile ee_s32 seed3_volatile=0x66;
#endif
#if PROFILE_RUN
	volatile ee_s32 seed1_volatile=0x8;
	volatile ee_s32 seed2_volatile=0x8;
	volatile ee_s32 seed3_volatile=0x8;
#endif
	volatile ee_s32 seed4_volatile=ITERATIONS;
	volatile ee_s32 seed5_volatile=0;
/* Porting : Timing functions
	How to capture time and convert to seconds must be ported to whatever is supported by the platform.
	e.g. Read value from on board RTC, read value from cpu clock cycles performance counter etc. 
	Sample implementation for standard time.h and windows.h definitions included.
*/
/* Define : TIMER_RES_DIVIDER
	Divider to trade off timer resolution and total time that can be measured.

	Use lower values to increase resolution, but make sure that overflow does not occur.
	If there are issues with the return value overflowing, increase this value.
	*/
#include "esp_timer.h"
#define CORETIMETYPE CORE_TICKS
#define GETMYTIME(_t) (*_t=esp_timer_get_time())
#define MYTIMEDIFF(fin,ini) ((fin)-(ini))
#define TIMER_RES_DIVIDER 1
#define SAMPLE_TIME_IMPLEMENTATION 1
#define EE_TICKS_PER_SEC (1000000LL)

/** Define Host specific (POSIX), or target specific global time variables. */
static CORETIMETYPE start_time_val, stop_time_val;

/* Function : start_time
	This function will be called right before starting the timed portion of the benchmark.

	Implementation may be capturing a system timer (as implemented in the example code) 
	or zeroing some system parameters - e.g. setting the cpu clocks cycles to 0.
*/
void start_time(void) {
    taskENTER_CRITICAL();
    GETMYTIME(&start_time_val);
    ee_printf("Debug: start_time = %d\n", (int)start_time_val);
    taskEXIT_CRITICAL();
}

void stop_time(void) {
    taskENTER_CRITICAL();
    GETMYTIME(&stop_time_val);
    ee_printf("Debug: stop_time = %d\n", (int)stop_time_val);
    taskEXIT_CRITICAL();
}

CORE_TICKS get_time(void) {
    CORE_TICKS elapsed = MYTIMEDIFF(stop_time_val, start_time_val);
    ee_printf("Debug: elapsed ticks = %d\n", (int)elapsed);
    return elapsed;
}

secs_ret time_in_secs(CORE_TICKS ticks) {
    ee_printf("Debug - ticks (raw): %lu\n", (unsigned long)ticks);
    ee_printf("Debug - EE_TICKS_PER_SEC (raw): %lu\n", (unsigned long)EE_TICKS_PER_SEC);
    
    secs_ret ticks_cast = (secs_ret)ticks;
    secs_ret hz_cast = (secs_ret)EE_TICKS_PER_SEC;
    
    ee_printf("Debug - ticks (cast to double): %.6f\n", (double)ticks_cast);
    ee_printf("Debug - EE_TICKS_PER_SEC (cast to double): %.6f\n", (double)hz_cast);
    
    secs_ret retval = ticks_cast / hz_cast;
    ee_printf("Debug - division result: %.6f\n", (double)retval);
    
    return retval;
}

ee_u32 default_num_contexts=1;

/* Function : portable_init
	Target specific initialization code 
	Test for some common mistakes.
*/
void portable_init(core_portable *p, int *argc, char *argv[])
{
	if (sizeof(ee_ptr_int) != sizeof(ee_u8 *)) {
		ee_printf("ERROR! Please define ee_ptr_int to a type that holds a pointer!\n");
	}
	if (sizeof(ee_u32) != 4) {
		ee_printf("ERROR! Please define ee_u32 to a 32b unsigned type!\n");
	}
	p->portable_id=1;
}
/* Function : portable_fini
	Target specific final code 
*/
void portable_fini(core_portable *p)
{
	p->portable_id=0;
}


