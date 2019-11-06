/*
 * This file is part of libgreat
 *
 * LPC43xx timer drivers
 */


#include <debug.h>
#include <drivers/timer.h>

#include <drivers/arm_vectors.h>
#include <drivers/platform_vectors.h>

/**
 * Platform specific data for the LPC43xx timer.
 */
struct {

	// The branch clock for the relevant timer.
	platform_branch_clock_t *clock;

	// Stores the interrupt corresponding to the relevant timer.
	platform_irq_t irq;

} platform_timer_data;


/**
 * Timer object for the timer reserved for system use.
 */
static hw_timer_t platform_timer = { .reg = NULL, .number = TIMER3 };

// When a given timer interrupt occurs, we'll want to be able to look
// up the timer object associated with it. This array will track the timers
// associated with the TIMER0-TIMER4 IRQs. Entries are only valid when a given
// timer's interrupt is enabled.
static hw_timer_t *timer_for_irq[SUPPORTED_TIMERS];

// Track which timers are in use.
bool timer_in_use[SUPPORTED_TIMERS];


/**
 * Attempts to reserve use of a timer that's not in use.
 *
 * @return a valid timer_index_t if one is available, or NO_TIMER_AVAILABLE otherwise.
 */
timer_index_t platform_reserve_free_timer(void)
{
	// If we have a free timer, reserve it, and return its index.
	for (unsigned i = 0; i < SUPPORTED_TIMERS; ++i) {
		if (!timer_in_use[i]) {
			timer_in_use[i] = true;
			return i;
		}
	}

	// Otherwise, return that there's no timer available.
	return NO_TIMER_AVAILABLE;
}


/**
 * Returns a timer reserved with platform_reserve_free_timer to the pool of available timers.
 *
 * @param index The number of the timer to be returned.
 */
void platform_release_timer(timer_index_t index)
{
	if (index >= SUPPORTED_TIMERS) {
		pr_error("error: timer: tried to free timer %d, which does not exist!\n", index);
		return;
	}

	timer_in_use[index] = false;
}






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
		default:
			pr_error("error: timer: trying to get an invalid timer %d\n", index);
			return NULL;
	}
}



/**
 * @returns the IRQ number for the given timer
 */
static platform_irq_t platform_get_irq_for_timer(timer_index_t index)
{
	switch(index)  {
		case TIMER0: return TIMER0_IRQ;
		case TIMER1: return TIMER1_IRQ;
		case TIMER2: return TIMER2_IRQ;
		case TIMER3: return TIMER3_IRQ;
		default:
			pr_error("error: timer: trying to get an invalid timer %d\n", index);
			return -1;
	}
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
		default:
			pr_error("error: timer: trying to get an invalid timer %d\n", index);
			return NULL;
	}
}



/**
 * Perform platform-specific initialization for an LPC43xx timer peripheral.
 *
 * @param timer The timer object to be initialized.
 * @param index The number of the timer to be set up.
 */
void platform_timer_initialize(hw_timer_t *timer, timer_index_t index)
{
	// Store the hardware assets relevant to the timer.
	timer->reg = platform_get_timer_registers(index);
	timer->platform_data.clock = platform_get_timer_clock(index);
	timer->platform_data.irq = platform_get_irq_for_timer(index);

	// ... and ensure the relevant clock is enabled.
	platform_enable_branch_clock(timer->platform_data.clock, false);

	// Disable all match functionality by default.
	timer->reg->match_control.all = 0;
}


/**
 * Identifies the clock divider necessary to most closely approximate a given frequency.
 */
static uint32_t compute_divider_for_frequency(hw_timer_t *timer, uint32_t frequency)
{
	platform_branch_clock_t *clock = platform_get_timer_clock(timer->number);

	// Identify the frequency of the timer's parent clock, and identify a divisor accordingly.
	uint32_t base_frequency = platform_get_branch_clock_frequency(clock);
	uint32_t target_divider = (double)base_frequency / (double)frequency;

	pr_debug("timer%d: parent clock frequency identified as to %" PRIu32 " Hz\n", timer->number, base_frequency);
	pr_debug("timer%d: divisor identified as %" PRIu32 "\n", timer->number, target_divider);

	return target_divider;
}


/**
 * Sets the frequency of the given timer. For the LPC43xx, this recomputes the timer's divider.
 *
 * @param timer The timer to be configured.
 * @param tick_frequency The timer's tick frequency, in Hz.
 */
void platform_timer_set_frequency(hw_timer_t *timer, uint32_t tick_frequency)
{
	uint32_t target_divider = compute_divider_for_frequency(timer, tick_frequency);

	// Apply our divisor in order to achieve as close as we can to our target output frequency.
	timer->reg->prescaler = target_divider - 1;
}


/**
 * Sets the frequency of the timer's match interrupt, but does not set the interrupt's
 * handler, or enable it in the NVIC. The interrupt should be disabled in the NVIC before callling this.
 */
void platform_timer_set_interrupt_frequency(hw_timer_t *timer, uint32_t event_frequency)
{
	match_control_register_t match_control = {};

	uint32_t target_divider = compute_divider_for_frequency(timer, event_frequency);

	pr_info("timer: using maximum count value of %u\n", target_divider);

	// For now, set the timer prescalar to unity, so we're only using the match timer.
	// This is likely good enough even for future modifications to this driver.
	timer->reg->prescaler= 0;

	// Set our match register so a match interrupt will occur at the given frequency...
	// TODO: enable using more than one match to allow for more than one periodic iinterrupt:
	timer->reg->match_value[0] = target_divider - 1;

	// Configure the timer to count up to the provided value, and then trigger an interrupt and start over.
	match_control.interrupt_on_match0 = true;
	match_control.reset_on_match0 = true;
	timer->reg->match_control = match_control;
}


/**
 * Enables the given timer. Typically, you want to configure the timer
 * beforehand with calls to e.g. platform_timer_set_frequency.
 */
void platform_timer_enable(hw_timer_t *timer)
{
	timer->reg->enabled = 1;
}


/**
 * Disables the given timer, and all associated events.
 */
void platform_timer_disable(hw_timer_t *timer)
{
	timer->reg->enabled = 0;

	// Also disable our associated interrupts.
	timer->reg->match_control.all = 0;
	platform_disable_interrupt(timer->platform_data.irq);
}


/**
 * @returns True iff the given timer is enabled.
 */
bool platform_timer_enabled(hw_timer_t *timer)
{
	return timer->reg->enabled;
}



/**
 * @returns the current counter value of the given timer
 */
uint32_t platform_timer_get_value(hw_timer_t *timer)
{
	return timer->reg->value;
}



/**
 * Sets up the system's platform timer.
 *
 * @returns A reference to the system's platform timer.
 */
hw_timer_t *platform_set_up_platform_timer(void)
{
	timer_in_use[platform_timer.number] = true;

	timer_initialize(&platform_timer, platform_timer.number);
	return &platform_timer;
}


/**
 * @returns A reference to the system's platform timer, or NULL if it has not yet been set up.
 */
hw_timer_t *platform_get_platform_timer(void)
{
	// If the platform timer hasn't been set up yet, enable it.
	if (!platform_timer.reg || !platform_timer_enabled(&platform_timer)) {
		return NULL;
	}

	return &platform_timer;
}


/**
 * Core timer interrupt handler.
 */
static void timer_interrupt_handler(hw_timer_t *timer)
{
	// Mark the relevant interrupt as serviced...
	timer->reg->interrupt_pending.match0 = 1;

	// ... and if we have an interval callback, call it.
	if(timer->interval_callback) {
		timer->interval_callback(timer->interval_callback_argument);
	}
}



/**
 * Timer interrupt trampolines.
 */
static void timer0_isr(void) { timer_interrupt_handler(timer_for_irq[0]); }
static void timer1_isr(void) { timer_interrupt_handler(timer_for_irq[1]); }
static void timer2_isr(void) { timer_interrupt_handler(timer_for_irq[2]); }
static void timer3_isr(void) { timer_interrupt_handler(timer_for_irq[3]); }


/**
 * Sets up a timer to handle any periodic callbacks associated with it.
 * Requires the platform-independent driver to have populated its interval callback fields.
 */
uint32_t platform_schedule_periodic_callbacks(hw_timer_t *timer)
{
	interrupt_service_routine_t isrs[] = {timer0_isr, timer1_isr, timer2_isr, timer3_isr};

	// Ensure our interrupt isn't active during our configuration.
	platform_disable_interrupt(timer->platform_data.irq);

	// ... set up the interrupt timing and handler...
	platform_timer_set_interrupt_frequency(timer, timer->callback_frequency);
	platform_set_interrupt_handler(timer->platform_data.irq, isrs[timer->number]);

	// ... associate the timer with the IRQ...
	timer_for_irq[timer->number] = timer;

	// ... and finally, enable the interrupt...
	platform_enable_interrupt(timer->platform_data.irq);

	// ... and the timer itself.
	timer->reg->enabled = 1;
	return 0;
}


/**
 * Cancels all periodic callbacks associated with a given timer.
 */
uint32_t platform_cancel_periodic_callbacks(hw_timer_t *timer)
{
	platform_timer_disable(timer);
}
