/*
 * This file is part of libgreat.
 *
 * System reset driver functionality.
 */

#ifndef __LIBGREAT_RESET_H__
#define __LIBGREAT_RESET_H__

#include <drivers/platform_reset.h>

typedef enum {
	// Keep these unique, so the RAM is unlikely to settle into these on first
	// boot.
	RESET_REASON_UNKNOWN           = 0xAA55FF00,
	RESET_REASON_SOFT_RESET	       = 0xAA55FF01,
	RESET_REASON_FAULT             = 0xAA55FF02,
	RESET_REASON_POWER_CYCLE       = 0xAA55FF03,
	RESET_REASON_WATCHDOG_TIMEOUT  = 0xAA55FF04,
	RESET_REASON_NEW_FIRMWARE      = 0xAA55FF05,
	RESET_REASON_USE_EXTCLOCK      = 0xAA55CCDD,

	RESET_REASON_LIKELY_VALID_MASK = 0xAA550000,
	RESET_MEMORY_LIKELY_VALID_MASK = 0xAA550000,
} reset_reason_t;


/**
 * @return true iff the system's memory seems likely ot have preserved its value since a prior operation
 */
bool system_persistent_memory_likely_intact(void);


/**
 * @return a constant indicating the reason for the last reset, if known
 */
reset_reason_t system_reset_reason(void);

/**
 * Resets the entire system.
 *
 * @param reason The reset reason to report.
 * @param include_always_on_domain True iff the always-on power domain should be included in the given reset.
 */
void system_reset(reset_reason_t reason, bool include_always_on_domain);

/**
 * @return a string describing the reason for the system's reset
 */
const char *system_get_reset_reason_string(void);

#endif
