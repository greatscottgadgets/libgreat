/*
 * This file is part of libgreat.
 *
 * System reset driver functionality.
 */

#include <toolchain.h>
#include <drivers/reset.h>
#include <drivers/platform_reset.h>

/* This special variable is preserved across soft resets by a little bit of
 * reset handler magic. It allows us to pass a Reason across resets. */
static volatile uint32_t reset_reason ATTR_PERSISTENT;

/* Reset reason from the last iteration. */
static uint32_t last_reset_reason = 0;


/**
 * Set up our system reset driver.
 */
void reset_driver_initialize(void)
{
	// Store the reset reason we gathered from the last iteration.
	last_reset_reason = reset_reason;
	reset_reason = RESET_REASON_UNKNOWN;

	// If all memory seems to be cleared out / corrupted, this was likely a power cycle;
	// set the last reset reason to "power cycle".
	if (!system_persistent_memory_likely_intact()) {
		last_reset_reason = RESET_REASON_POWER_CYCLE;
	}

	// Let the platform handle any initialization it needs to
	platform_initialize_reset_driver();
}
CALL_ON_PREINIT(reset_driver_initialize);


/**
 * @return true iff the system's memory seems likely ot have preserved its value since a prior operation
 */
bool system_persistent_memory_likely_intact(void)
{
	// If we've already overwritten the reset reason, use our stored one; otherewise use our persistent value directly.
	// This ensures that we return a reasonable value no matter if we're called before or after our driver is initialized.
	uint32_t reset_reason_to_use = (reset_reason == RESET_REASON_UNKNOWN) ? last_reset_reason : reset_reason;

	// Check if the semaphore bits in the reset reason were set to our "known valid" mask -- this gives us a high
	// probaility that we explicility set this in a previous iteration.
	return (reset_reason_to_use & RESET_MEMORY_LIKELY_VALID_MASK) == RESET_MEMORY_LIKELY_VALID_MASK;
}


/**
 * @return a string describing the reason for the system's reset
 */
const char *system_get_reset_reason_string(void)
{
	// If we've already overwritten the reset reason, use our stored one; otherewise use our persistent value directly.
	// This ensures that we return a reasonable value no matter if we're called before or after our driver is initialized.
	uint32_t reset_reason_to_use = (reset_reason == RESET_REASON_UNKNOWN) ? last_reset_reason : reset_reason;

	switch (reset_reason_to_use) {
		case RESET_REASON_POWER_CYCLE:
			return "power cycle";
		case RESET_REASON_SOFT_RESET:
			return "software reset";
		case RESET_REASON_USE_EXTCLOCK:
			return "reset to switch to external clock";
		case RESET_REASON_FAULT:
			return "fault-induced reset";
		case RESET_REASON_WATCHDOG_TIMEOUT:
			return "watchdog timeout";
		case RESET_REASON_NEW_FIRMWARE:
			return "firmware re-flash.";
		default:
			if (system_persistent_memory_likely_intact()) {
				return "unknown (non-power-cycle) reset";
			} else {
				return "hard reset / power cycle";
			}
	}
}



/**
 * @return a constant indicating the reason for the last reset, if known
 */
reset_reason_t system_reset_reason(void)
{
	// If our persistent memory is likely intact, we can use its reset reason.
	if (system_persistent_memory_likely_intact()) {
		return last_reset_reason;
	}

	// Otherewise, we don't know why the system was reset; indicate so.
	return RESET_REASON_UNKNOWN;
}


/**
 * Resets the entire system.
 *
 * @param reason The reset reason to report.
 * @param include_always_on_domain True iff the always-on power domain should be included in the given reset.
 */
ATTR_NORETURN void system_reset(reset_reason_t reason, bool include_always_on_domain)
{
	reset_reason = reason;
	platform_software_reset(include_always_on_domain);

	while(1);
}


