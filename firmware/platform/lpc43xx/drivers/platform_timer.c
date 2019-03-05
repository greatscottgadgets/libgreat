/*
 * This file is part of libgreat
 *
 * LPC43xx timer drivers
 */

// Temporary debug.
#define LOCAL_FILE_OVERRIDE_LOGLEVEL


#include <debug.h>
#include <drivers/timer.h>

#include <drivers/platform_clock.h>

/**
 * Timer object for the timer reserved for system use.
 */
static timer_t platform_timer = { .reg = NULL, .number = TIMER3 };


/**
 * @returns a reference to the register bank for the given timer index.
 */
static platform_timer_registers_t *platform_get_timer_registers(timer_index_t index)
{
	switch(index)  {
		case TIMER0: return (platform_timer_registers_t *)0x40084000;
		case TIMER1: return (platform_timer_registers_t *)0x40085000;
		case TIMER2: return (platform_timer_registers_t *)0x400C3000;
		case TIMER3: return (platform_timer_registers_t *)0x400C4000;
	}

	return NULL;
}


/**
 * @returns A reference to the clock that controls the given timer.
 */
static platform_branch_clock_t *platform_get_timer_clock(timer_index_t index)
{
	platform_clock_control_register_block_t *ccu = get_platform_clock_control_registers();

	switch(index)  {
		case TIMER0: return &ccu->m4.timer0;
		case TIMER1: return &ccu->m4.timer1;
		case TIMER2: return &ccu->m4.timer2;
		case TIMER3: return &ccu->m4.timer3;
	}

	return NULL;
}



/**
 * Perform platform-specific initialization for an LPC43xx timer peripheral.
 *
 * @param timer The timer object to be initialized.
 * @param index The number of the timer to be set up.
 */
void platform_timer_initialize(timer_t *timer, timer_index_t index)
{
    // Figure out the clock that drives the given timer, and the register bank that controls it.
	platform_timer_registers_t *reg = platform_get_timer_registers(index);
	platform_branch_clock_t *clock  = platform_get_timer_clock(index);

	// Store a reference to the timer registers...
	timer->reg = reg;

	// ... and ensure the relevant clock is enabled.
	platform_enable_branch_clock(clock, false);
}


/**
 * Sets the frequency of the given timer. For the LPC43xx, this recomputes the timer's divider.
 *
 * @param timer The timer to be configured.
 * @param tick_freuqency The timer's tick frequency, in Hz.
 */
void platform_timer_set_frequency(timer_t *timer, uint32_t tick_frequency)
{
	platform_branch_clock_t *clock = platform_get_timer_clock(timer->number);

	// Identify the frequency of the timer's parent clock, and identify a divisor accordingly.
	uint32_t base_frequency = platform_get_branch_clock_frequency(clock);
	uint32_t target_divider = (double)base_frequency / (double)tick_frequency;

	pr_debug("timer%d: parent clock frequency identified as to %" PRIu32 " Hz\n", timer->number, base_frequency);
	pr_debug("timer%d: divisor identified as %" PRIu32 "\n", timer->number, target_divider);

	// Apply our divisor in order to achieve as close as we can to our target output frequency.
	timer->reg->prescaler = target_divider - 1;
}


/**
 * Enables the given timer. Typically, you want to configure the timer
 * beforehand with calls to e.g. platform_timer_set_frequency.
 */
void platform_timer_enable(timer_t *timer)
{
	timer->reg->enabled = 1;
}


/**
 * Disables the given timer.
 */
void platform_timer_disable(timer_t *timer)
{
	timer->reg->enabled = 0;
}


/**
 * @returns True iff the given timer is enabled.
 */
bool platform_timer_enabled(timer_t *timer)
{
	return timer->reg->enabled;
}



/**
 * @returns the current counter value of the given timer
 */
uint32_t platform_timer_get_value(timer_t *timer)
{
	return timer->reg->value;
}



/**
 * Sets up the system's platform timer.
 *
 * @returns A reference to the system's platform timer.
 */
timer_t *platform_set_up_platform_timer(void)
{
	timer_initialize(&platform_timer, platform_timer.number);
	return &platform_timer;
}


/**
 * @returns A reference to the system's platform timer, or NULL if it has not yet been set up.
 */
timer_t *platform_get_platform_timer(void)
{
	// If the platform timer hasn't been set up yet, enable it.
	if (!platform_timer.reg || !platform_timer_enabled(&platform_timer)) {
		return NULL;
	}

	return &platform_timer;
}
