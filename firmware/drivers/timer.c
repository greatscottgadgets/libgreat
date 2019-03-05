/*
 * This file is part of libgreat
 *
 * Core system timer drivers
 */

// Temporary debug.
#define LOCAL_FILE_OVERRIDE_LOGLEVEL

#include <debug.h>
#include <drivers/timer.h>


/**
 * Initializes a timer peripheral.
 *
 * @param timer The timer object to be initialized.
 * @param index The number of the timer to be set up.
 */
void timer_initialize(timer_t *timer, timer_index_t index)
{
	timer->number = index;

	// Perform platform-specific timer initialization.
	platform_timer_initialize(timer, index);
}


/**
 * Enables the given timer and sets it to tick at a given frequency.
 */
void timer_enable(timer_t *timer, uint32_t tick_frequency)
{
	// Store the timer's frequency, for later use.
	timer->frequency = tick_frequency;

	// Set the timer up, and enable it.
	platform_timer_set_frequency(timer, tick_frequency);
	platform_timer_enable(timer);
}


/**
 * @returns the current counter value of the given timer
 */
uint32_t timer_get_value(timer_t *timer)
{
	return platform_timer_get_value(timer);
}


/**
 * Function that must be called whenever the clock driving the given timer experiences a change in frequency.
 * This allows the timer to automatically recompute its period. There may be some loss of ticks during the clock
 * frequency change.
 */
void timer_handle_clock_frequency_change(timer_t * timer)
{
	// Update the timer's internal frequency.
	platform_timer_set_frequency(timer, timer->frequency);
}


/**
 * Initialization function for the platform microsecond timer, which is used
 * to track runtime microseconds.
 */
void set_up_platform_timers(void)
{
	timer_t *timer = platform_set_up_platform_timer();

	// Enable the timer, with a frequency of a millisecond.
	timer_enable(timer, 1000000UL);
}


/**
 * @returns the total number of microseconds since this timer was initialized.
 *
 * Overflows roughly once per hour. For tracking longer spans; use the RTC
 * functions, which are currently not synchronized to this one.
 */
uint32_t get_time(void)
{
	// Return the value on the platform timer.
	timer_t *timer = platform_get_platform_timer();
	return timer_get_value(timer);
}


/**
 * @returns The total number of microseconds that have passed since a reference call to get_time().
 *		Useful for computing timeouts.
 */
uint32_t get_time_since(uint32_t base)
{
	return get_time() - base;
}


/**
 * Function that should be called whenever the platform timer's basis changes.
 * // FIXME: remove this!
 */
void handle_platform_timer_frequency_change(void)
{
	timer_t *platform_timer = platform_get_platform_timer();

	if (!platform_timer) {
		return;
	}

	timer_handle_clock_frequency_change(platform_timer);
}


/**
 * Blocks execution for the provided number of microseconds.
 */
void delay_us(uint32_t duration)
{
	if (!platform_get_platform_timer())
	{
		pr_critical("critical: tried to get the platform timer before it was up!\n");
		while(1);
	}

	uint32_t time_base = get_time();
	while(get_time_since(time_base) < duration);
}



