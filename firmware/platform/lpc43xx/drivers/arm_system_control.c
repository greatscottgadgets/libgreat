/*
 * This file is part of libgreat.
 *
 * ARM system control drivers.
 */

#include <drivers/arm_system_control.h>

/**
 * @return a reference to the ARM SCB.
 */
arm_system_control_register_block_t *arch_get_system_control_registers(void)
{
	return (arm_system_control_register_block_t *)0xE000ED00;
}


/**
 * Enables access to the system's FPU.
 *
 * @param allow_unprivileged_access True iff user-mode should be able to use the FPU.
 */
void arch_enable_fpu(bool allow_unprivileged_access)
{
	arm_system_control_register_block_t *scb = arch_get_system_control_registers();
	fpu_access_rights_t access = allow_unprivileged_access ? FPU_FULL_ACCESS : FPU_PRIVILEGED_ONLY;

	scb->cpacr.fpu_access = access;
}
