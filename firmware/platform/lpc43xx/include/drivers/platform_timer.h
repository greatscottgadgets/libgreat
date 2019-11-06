/*
 * This file is part of libgreat
 *
 * LPC43xx timer drivers
 */

#ifndef __LIBGREAT_PLATFORM_TIMERS_H__
#define __LIBGREAT_PLATFORM_TIMERS_H__

#include <toolchain.h>

#include <drivers/platform_clock.h>

typedef struct timer hw_timer_t;

/**
 * Timer numbers for each of the LPC43xx timer peripherals.
 */
typedef enum {
	TIMER0 = 0,
	TIMER1 = 1,
	TIMER2 = 2,
	TIMER3 = 3,

	// Meta: the total number of timers.
	SUPPORTED_TIMERS = 4,

	// Flag: used to indicate that no timer is available.
	NO_TIMER_AVAILABLE = -1
} timer_index_t;


/**
 * Platform specific data for the LPC43xx timer.
 */
typedef struct {

	// The branch clock for the relevant timer.
	platform_branch_clock_t *clock;

	// Stores the interrupt corresponding to the relevant timer.
	// FIXME: this should be a platform_irq_t, but we can't switch to this until the USB driver uses libgreat vector drivers
	uint32_t irq;

} platform_timer_data_t;


/**
 * Model of the register that controls timer match event behaviors.
 */
typedef union {
	struct {
		uint32_t interrupt_on_match0 : 1;
		uint32_t reset_on_match0     : 1;
		uint32_t stop_on_match0      : 1;

		uint32_t interrupt_on_match1 : 1;
		uint32_t reset_on_match1     : 1;
		uint32_t stop_on_match1      : 1;

		uint32_t interrupt_on_match2 : 1;
		uint32_t reset_on_match2     : 1;
		uint32_t stop_on_match2      : 1;

		uint32_t interrupt_on_match3 : 1;
		uint32_t reset_on_match3     : 1;
		uint32_t stop_on_match3      : 1;
	};
	uint32_t all;
} match_control_register_t;



/**
 * Register layout for LPC43xx timers.
 */
typedef volatile struct ATTR_PACKED {

	// Interrupt register.
	struct {
		// Match channels.
		uint32_t match0   : 1;
		uint32_t match1   : 1;
		uint32_t match2   : 1;
		uint32_t match3   : 1;

		// Capture channels.
		uint32_t capture0 : 1;
		uint32_t capture1 : 1;
		uint32_t capture2 : 1;
		uint32_t capture3 : 1;

		uint32_t          : 24;
	} interrupt_pending;

	// Timer control.
	struct {
		uint32_t enabled  :  1;
		uint32_t reset    :  1;
		uint32_t          : 30;
	};

	// Current counter value ("counter").
	uint32_t value;

	// Prescale registers.
	uint32_t prescaler;
	uint32_t prescale_counter;

	// Control registers for matching hardware.
	match_control_register_t match_control;
	uint32_t match_value[4];

	// Control registers for the capture hardware.
	uint32_t capture_control;
	uint32_t captured_value[4];

	// Control registers for matching against external pins.
	uint32_t external_match_register;

	RESERVED_WORDS(12);

	// Counter control: controls using the timer/counter to count events,
	// rather than continuously counting.
	struct {

		// Determines the core behavior of the timer: should it count unconditionally (timer mode),
		// or should it count events? (Takes a value from platform_timer_counter_mode_t.)
		uint32_t counter_mode : 2;

		// The input that will drive counter events.
		// Selected from the capture inputs.
		uint32_t counter_input : 2;

		uint32_t               : 28;

	} count_control_register;

} platform_timer_registers_t;


ASSERT_OFFSET(platform_timer_registers_t, value,                   0x08);
ASSERT_OFFSET(platform_timer_registers_t, capture_control,         0x28);
ASSERT_OFFSET(platform_timer_registers_t, external_match_register, 0x3c);
ASSERT_OFFSET(platform_timer_registers_t, count_control_register,  0x70);


/**
 * Counter mode for the LPC43xx counter peripherals.
 *
 * These will be used by the downstream Counter driver for the LPC43xx,
 * when it's completed. We'll link that here, then.
 */
typedef enum {
	TIMER_COUNT_PRESCALER_PERIODS   = 0,
	TIMER_COUNT_EVENT_RISING_EDGES  = 1,
	TIMER_COUNT_EVENT_FALLING_EDGES = 2,
	TIMER_COUNT_EVENT_EDGES         = 3,
} timer_counter_mode_t;


#endif
