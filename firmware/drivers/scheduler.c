/**
 * Simple co-operative round-robin scheduler functionality for GreatFET.
 * This file is part of libgreat
 */

#include <toolchain.h>

// TODO: implement task state, yielding, and magic?

// Definitions that let us get at our list of tasks.
typedef void (*task_implementation_t) (void);
extern task_implementation_t __task_array_start, __task_array_end;



/**
 * Runs a single iteration of each defined task (a single scheduler "round")
 * For an variant that runs indefinitely, use scheduler_run().
 */
void scheduler_run_tasks(void)
{
	task_implementation_t *task;

	// Execute each task in our list, once.
	for (task = &__task_array_start; task < &__task_array_end; task++) {
		(*task)();
	}
}

/**
 * Runs our round-robin scheduler for as long as the device is alive; never returns.
 */
ATTR_NORETURN void scheduler_run(void)
{
	while(1) {
		scheduler_run_tasks();
	}
}
