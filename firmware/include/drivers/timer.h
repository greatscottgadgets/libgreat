/*
 * This file is part of libgreat
 *
 * Core system timer drivers
 */

#ifndef __LIBGREAT_TIMER_H__
#define __LIBGREAT_TIMER_H__

#include <drivers/platform_timer.h>

/**
 * Struct representing a timer peripheral.
 */
typedef struct timer {

	// The timer number for the given timer.
	timer_index_t number;

	// Reference to the timer control register bank.
	platform_timer_registers_t *reg;

	// Timer's frequency, in Hz.
	// This is the frequency at which the timer counts.
	uint32_t frequency;

} timer_t;


/**
 * Initializes a timer peripheral. Does not configure or enable the timer.
 *
 * @param timer The timer object to be initialized.
 * @param index The number of the timer to be set up. Platform-specific, but usually a 0-indexed integer.
 */
void timer_initialize(timer_t *timer, timer_index_t index);

/**
 * Initialization function for the platform microsecond timer, which is used
 * to track runtime microseconds.
 */
void set_up_platform_timers(void);


/**
 * Blocks execution for the provided number of microseconds.
 */
void delay_us(uint32_t duration);

/**
 * @returns the total number of microseconds since this timer was initialized.
 *
 * Overflows roughly once per hour. For tracking longer spans; use the RTC
 * functions, which are currently not synchronized to this one.
 */
uint32_t get_time(void);


/**
 * @returns The total number of microseconds that have passed since a reference call to get_time().
 *		Useful for computing timeouts.
 */
uint32_t get_time_since(uint32_t base);


/**
 * Function that should be called whenever the platform timer's basis changes.
 * FIXME: remove this!
 */
void handle_platform_timer_frequency_change(void);

#endif
