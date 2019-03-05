/**
 * This file is part of libgreat
 *
 * LPC43xx reset generation/control driver
 */

#include <drivers/reset.h>
#include <drivers/platform_reset.h>

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
	rgu->core_reset = 1;
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
