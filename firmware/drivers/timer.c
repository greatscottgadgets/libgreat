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
 * Attempts to reserve use of a timer from the pool of timers that are not in use.
 * Timer is automatically initialized on acquisition, but not configured to a given
 * frequency or enabled.
 *
 * @param timer A timer object that will be populated automatically once acquired.
 * @return 0 on success, or an error code if no timer could be acquired
 */
uint32_t acquire_timer(hw_timer_t *timer)
{
	timer_index_t index = platform_reserve_free_timer();

	if (index == NO_TIMER_AVAILABLE) {
		return EBUSY;
	}

	// Perform platform-specific timer initialization.
	platform_timer_initialize(timer, index);
	return 0;
}


/**
 * Initializes a timer peripheral.
 *
 * @param timer The timer object to be initialized.
 * @param index The number of the timer to be set up.
 */
void timer_initialize(hw_timer_t *timer, timer_index_t index)
{
	timer->number = index;

	// Perform platform-specific timer initialization.
	platform_timer_initialize(timer, index);
}


/**
 * Enables the given timer and sets it to tick at a given frequency.
 */
void timer_enable(hw_timer_t *timer, uint32_t tick_frequency)
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
uint32_t timer_get_value(hw_timer_t *timer)
{
	return platform_timer_get_value(timer);
}


/**
 * Function that must be called whenever the clock driving the given timer experiences a change in frequency.
 * This allows the timer to automatically recompute its period. There may be some loss of ticks during the clock
 * frequency change.
 */
void timer_handle_clock_frequency_change(hw_timer_t * timer)
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
	hw_timer_t *timer = platform_set_up_platform_timer();

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
	hw_timer_t *timer = platform_get_platform_timer();
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
	hw_timer_t *platform_timer = platform_get_platform_timer();

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


/**
 * Schedules a given function to execute periodically. Assumes the caller is not using the timer for anything else.
 *
 * @param frequency The frequency with which the function should be called.
 * @param function The function to be called. Should return void and accept a void*.
 * @param argument The argument to be provided to the given function.
 */
uint32_t call_function_periodically(hw_timer_t *timer, uint32_t frequency, timer_callback_t function, void *argument)
{
	timer->callback_frequency = frequency;

	timer->interval_callback = function;
	timer->interval_callback_argument = argument;

	return platform_schedule_periodic_callbacks(timer);
}


/**
 * Cancels all periodic function calls associated with a given timer.
 */
uint32_t cancel_periodic_function_calls(hw_timer_t *timer)
{
	platform_cancel_periodic_callbacks(timer);
	return 0;
}



/**
 * Releases a timer reserved with acquire_timer.
 */
void release_timer(hw_timer_t *timer)
{
	platform_timer_disable(timer);
	platform_release_timer(timer->number);
}

