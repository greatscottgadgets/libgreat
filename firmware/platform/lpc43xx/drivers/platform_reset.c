/**
 * This file is part of libgreat
 *
 * LPC43xx reset generation/control driver
 */

#include <drivers/reset.h>
#include <drivers/platform_reset.h>
#include <drivers/platform_config.h>

/**
 * Return a reference to the LPC43xx's RGU block.
 */
platform_reset_register_block_t *get_platform_reset_registers(void)
{
	return (platform_reset_register_block_t *)0x40053000;
}


/**
 * Returns a reference to the LPC43xx's watchdog timer block.
 */
platform_watchdog_register_block_t *get_platform_watchdog_registers()
{
	return (platform_watchdog_register_block_t *)0x40080000;
}


/**
 * Reset everything except for the always-on / RTC power domain.
 */
static void platform_core_reset(void)
{
	platform_reset_register_block_t *rgu = get_platform_reset_registers();
	reset_select_t reset_select = { .core_reset = 1 };

	rgu->reset_control = reset_select;
}


/**
 * Feed the platform's watchdog timer, noting that the system is still alive.
 */
void platform_watchdog_feed(void)
{
	platform_watchdog_register_block_t *wwdt = get_platform_watchdog_registers();

	// Issue the write sequence that should feed the watchdog.
	wwdt->feed = 0xAA;
	wwdt->feed = 0x55;
}


/**
 * Reset everything including the always-on / RTC power domain.
 */
static void platform_watchdog_reset(void)
{
	const uint32_t default_watchdog_timeout = 100000;

	platform_watchdog_register_block_t *wwdt = get_platform_watchdog_registers();

	wwdt->enable = 1;
	wwdt->reset_enable = 1;
	wwdt->timeout = default_watchdog_timeout;

	platform_watchdog_feed();
}


/**
 * Software reset the entire system.
 *
 * @param true iff the always-on power domain should be included
 */
void platform_software_reset(bool include_always_on_domain)
{
	if (include_always_on_domain) {
		platform_watchdog_reset();
	} else {
		platform_core_reset();
	}
}


/**
 * @return true iff the system reset was an unintentional watchdog reset
 * 		tries to ignore cases where a soft-reset used the watchdog to implement the reset itself
 */
bool platform_reset_was_watchdog_timeout(void)
{
	platform_watchdog_register_block_t *wwdt = get_platform_watchdog_registers();
	reset_reason_t reported_reset_reason = system_reset_reason();

	// If the watchdog didn't time out in the previous iteration, this can't be a watchdog timeout.
	if (!wwdt->timed_out) {
		return false;
	}

	// If the watchdog did time out, this wasn't necessarily a true watchdog timeout, as sometimes
	// we use the a watchdog reset to trigger a full system reset from software.
	switch (reported_reset_reason) {

		// Filter out faults and soft-resets, as those both can trigger the WWDT,
		// and are not direct watchdog timer resets.
		case RESET_REASON_FAULT:
		case RESET_REASON_SOFT_RESET:
			return false;

		default:
			return true;
	}
}

/**
 * Clears any system state necessary to track the system's state across resets.
 */
void platform_initialize_reset_driver(void)
{
	platform_watchdog_register_block_t *wwdt = get_platform_watchdog_registers();
	wwdt->timed_out = 0;
}


/**
 * Configures the M0 app (primary M0 processor) to run, and starts it.
 */
void platform_halt_m0_core(void)
{
	platform_reset_register_block_t *rgu = get_platform_reset_registers();
	reset_select_t reset_select = { .m0app_reset = 1 };

	// Place the M0 into reset, and leave it there.
	rgu->reset_control = reset_select;
}


/**
 * Configures the M0 app (primary M0 processor) to run, and starts it.
 */
void platform_start_m0_core(void * m0_memory_base)
{
	platform_configuration_registers_t *creg = get_platform_configuration_registers();
	platform_reset_register_block_t *rgu = get_platform_reset_registers();
	reset_select_t reset_select = { .m0app_reset = 0 };

	// Ensure that the M0 is held in reset as we modify its base.
	platform_halt_m0_core();

	// Set the base for the M0 memory region...
	creg->m0app_shadow_base = (volatile uint32_t)m0_memory_base;

	// ... and bring it out of reset.
	rgu->reset_control = reset_select;
}
