/**
 * Simple co-operative round-robin scheduler functionality for GreatFET.
 * This file is part of libgreat.
 */


#include <toolchain.h>

#ifndef __LIBGREAT_SCHEDULER_H__
#define __LIBGREAT_SCHEDULER_H__



/**
 * Runs a single iteration of each defined task (a single scheduler "round")
 * For an variant that runs indefinitely, use scheduler_run().
 */
void scheduler_run_tasks(void);

/**
 * Runs our round-robin scheduler for as long as the device is alive; never returns.
 */
ATTR_NORETURN void scheduler_run(void);

#endif
