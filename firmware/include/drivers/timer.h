/*
 * This file is part of libgreat
 *
 * Core system timer drivers
 */

#ifndef __LIBGREAT_TIMER_H__
#define __LIBGREAT_TIMER_H__

#include <drivers/platform_timer.h>


/**
 * Type representing a function called by a timer; e.g. once per timer interval.
 */
typedef void (*timer_callback_t)(void *user_data);


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

	// A callback function that can be called at periodic intervals.
	// TODO: This potentially should be a list of callback functions, and their conditions.
	uint32_t callback_frequency;
	timer_callback_t interval_callback;
	void *interval_callback_argument;

	// Platform-specific data.
	platform_timer_data_t platform_data;

} hw_timer_t;



/**
 * Attempts to reserve use of a timer from the pool of timers that are not in use.
 * Timer is automatically initialized on acquisition, but not configured to a given
 * frequency or enabled.
 *
 * @param timer A timer object that will be populated automatically once acquired.
 * @return 0 on success, or an error code if no timer could be acquired
 */
uint32_t acquire_timer(hw_timer_t *timer);


/**
 * Releases a timer reserved with acquire_timer.
 */
void release_timer(hw_timer_t *timer);


/**
 * Initializes a timer peripheral. Does not configure or enable the timer.
 *
 * @param timer The timer object to be initialized.
 * @param index The number of the timer to be set up. Platform-specific, but usually a 0-indexed integer.
 */
void timer_initialize(hw_timer_t *timer, timer_index_t index);

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
 * Schedules a given function to execute periodically.
 *
 * @param frequency The frequency with which the function should be called.
 * @param function The function to be called. Should return void and accept a void*.
 * @param argument The argument to be provided to the given function.
 */
uint32_t call_function_periodically(hw_timer_t *timer, uint32_t frequency, timer_callback_t function, void *argument);


/**
 * Cancels all periodic function calls associated with a given timer.
 */
uint32_t cancel_periodic_function_calls(hw_timer_t *timer);


/**
 * Function that should be called whenever the platform timer's basis changes.
 * FIXME: remove this!
 */
void handle_platform_timer_frequency_change(void);


//
// Platform-specific timer code. These functions should be implemented by the target driver.
//


/**
 * Perform platform-specific initialization for a timer peripheral.
 *
 * @param timer The timer object to be initialized.
 * @param index The number of the timer to be set up.
 */
void platform_timer_initialize(hw_timer_t *timer, timer_index_t index);


/**
 * Sets the frequency of the given timer, this recomputes the timer's divider.
 *
 * @param timer The timer to be configured.
 * @param tick_frequency The timer's tick frequency, in Hz.
 */
void platform_timer_set_frequency(hw_timer_t *timer, uint32_t tick_frequency);


/**
 * Enables the given timer. Typically, you want to configure the timer
 * beforehand with calls to e.g. platform_timer_set_frequency.
 */
void platform_timer_enable(hw_timer_t *timer);


/**
 * Disables the given timer.
 */
void platform_timer_disable(hw_timer_t *timer);


/**
 * @returns the current counter value of the given timer
 */
uint32_t platform_timer_get_value(hw_timer_t *timer);


/**
 * @returns A reference to the system's platform timer -- initializing the relevant timer, if needed.
 */
hw_timer_t *platform_get_platform_timer(void);


/**
 * Sets up the system's platform timer.
 *
 * @returns A reference to the system's platform timer.
 */
hw_timer_t *platform_set_up_platform_timer(void);


/**
 * Sets up a timer to handle any periodic callbacks associated with it.
 * Requires the platform-independent driver to have populated its interval callback fields.
 */
uint32_t platform_schedule_periodic_callbacks(hw_timer_t *timer);


/**
 * Cancels all periodic callbacks associated with the given timer.
 */
uint32_t platform_cancel_periodic_callbacks(hw_timer_t *timer);


/**
 * Attempts to reserve use of a timer that's not in use.
 *
 * @return a valid timer_index_t if one is available, or NO_TIMER_AVAILABLE otherwise.
 */
timer_index_t platform_reserve_free_timer(void);


/**
 * Returns a timer reserved with platform_reserve_free_timer to the pool of available timers.
 *
 * @param index The number of the timer to be returned.
 */
void platform_release_timer(timer_index_t index);


#endif
