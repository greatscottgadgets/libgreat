/*
 * ARM exception and interrupt handling code.
 *
 * This file is part of libgreat
 */


#include <toolchain.h>
#include <drivers/arm_vectors.h>

/**
 * Definition for the ARM Nested Vectored Interrupt Controller (NVIC) register set.
 */
typedef volatile struct ATTR_WORD_ALIGNED {

	// Registers which set the relevant interrupt as enabled.
	uint32_t interrupt_enable[8];
	RESERVED_WORDS(24);
	uint32_t interrupt_disable[8];
	RESERVED_WORDS(24);

	uint32_t mark_interrupt_pending[8];
	RESERVED_WORDS(24);
	uint32_t mark_interrupt_serviced[8];
	RESERVED_WORDS(24);

	uint32_t interrupt_active[8];
	RESERVED_WORDS(56);

	uint32_t interrupt_priority[60];
	RESERVED_WORDS(644);

	uint32_t software_interrupt_trigger;

} arm_nvic_register_bank_t;

ASSERT_OFFSET(arm_nvic_register_bank_t, interrupt_disable,          0x080);
ASSERT_OFFSET(arm_nvic_register_bank_t, mark_interrupt_pending,     0x100);
ASSERT_OFFSET(arm_nvic_register_bank_t, mark_interrupt_serviced,    0x180);
ASSERT_OFFSET(arm_nvic_register_bank_t, interrupt_active,           0x200);
ASSERT_OFFSET(arm_nvic_register_bank_t, interrupt_priority,         0x300);
ASSERT_OFFSET(arm_nvic_register_bank_t, software_interrupt_trigger, 0xe00);


/**
 * @return A reference to the NVIC register bank.
 */
static arm_nvic_register_bank_t *get_nvic_registers(void)
{
	return (arm_nvic_register_bank_t *)0xE000E100UL;
}


/**
 * @return The offset into each NVIC register cluster that contains the relevant IRQ.
 */
static unsigned nvic_register_offset(platform_irq_number_t irq)
{
	return irq / 32;
}

/**
 * @return The bit in the relevant NVIC register that corresponds to the given IRQ.
 */
static uint32_t nvic_register_mask(platform_irq_number_t irq)
{
	return 1 << (irq % 32);
}


/**
 * Writes a mask to the given "mask register" -- that is, a register that accepts a bit-mask to
 * set or clear an interrupt property. Examples include the "interrupt set-enable" and "clear-enable" registers.
 *
 * @param group The register group to write to -- that is, the array of registers to be affected.
 * @param irq The IRQ whose value in the register should be affected.
 */
static void nvic_write_to_mask_register(volatile uint32_t *group, platform_irq_t irq)
{
	group[nvic_register_offset(irq)] = nvic_register_mask(irq);
}

/**
 * Writes a mask to the given "mask register" -- that is, a register that accepts a bit-mask to
 * set or clear an interrupt property. Examples include the "interrupt set-enable" and "clear-enable" registers.
 *
 * @param group The register group to write to -- that is, the array of registers to be affected.
 * @param irq The IRQ whose value in the register should be affected.
 */
static uint32_t nvic_read_from_mask_register(volatile uint32_t *group, platform_irq_t irq)
{
	return group[nvic_register_offset(irq)] & nvic_register_mask(irq);
}


/**
 * Enables the provided register in the system's interrupt controller.
 */
void platform_enable_interrupt(platform_irq_t irq)
{
	arm_nvic_register_bank_t *regs = get_nvic_registers();
	nvic_write_to_mask_register(regs->interrupt_enable, irq);
}


/**
 * Enables the provided register in the system's interrupt controller.
 */
void platform_disable_interrupt(platform_irq_t irq)
{
	arm_nvic_register_bank_t *regs = get_nvic_registers();
	nvic_write_to_mask_register(regs->interrupt_disable, irq);
}


/**
 * Marks an interrupt as actively pending.
 */
void platform_mark_interrupt_pending(platform_irq_t irq)
{
	arm_nvic_register_bank_t *regs = get_nvic_registers();
	nvic_write_to_mask_register(regs->mark_interrupt_pending, irq);
}


/**
 * Marks an interrupt as not currently actively-pending.
 */
void platform_mark_interrupt_serviced(platform_irq_t irq)
{
	arm_nvic_register_bank_t *regs = get_nvic_registers();
	nvic_write_to_mask_register(regs->mark_interrupt_serviced, irq);
}


/**
 * Marks an interrupt as not currently actively-pending.
 */
bool platform_interrupt_is_pending(platform_irq_t irq)
{
	arm_nvic_register_bank_t *regs = get_nvic_registers();
	return nvic_read_from_mask_register(regs->mark_interrupt_serviced, irq) ? 1 : 0;
}


/**
 * Sets the priority of a given interrupt.
 */
void platform_set_interrupt_priority(platform_irq_t irq, platform_interrupt_priority_t priority)
{
	arm_nvic_register_bank_t *regs = get_nvic_registers();

	// Get an alias that allows us to access each byte of the interrupt priority registers.
	uint8_t *interrupt_priority_bytes = (uint8_t *)regs->interrupt_priority;

	// ... and use that to set the priority.
	interrupt_priority_bytes[irq] = priority;
}


/**
 * Installs an interrupt handler routine for a given IRQ. Should only be called while a given interrupt is disabled.
 */
void platform_set_interrupt_handler(platform_irq_t irq, interrupt_service_routine_t isr)
{
	vector_table.irqs[irq] = isr;
}


// FIXME: The core vector table should have a _definition_ here, but we can't provide that while libopencm3
// wants to provide its own definition. It should be defined here when libopencm3 is removed.
