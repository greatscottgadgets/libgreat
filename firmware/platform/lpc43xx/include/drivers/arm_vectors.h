/*
 * ARM exception and interrupt handling code.
 *
 * This file is part of libgreat
 */

#ifndef __LIBGREAT_DRIVER_ARM_VECTORS_H__
#define __LIBGREAT_DRIVER_ARM_VECTORS_H__

#include <toolchain.h>
#include <drivers/platform_vectors.h>

/**
 * Type for vector table entries, which are just nullary functions.
 */
typedef void (*vector_table_entry_t)(void);

/**
 * Type describing an ARM vector table.
 */
typedef struct ATTR_PACKED {

	// The initial stack pointer value.
	unsigned int *initial_sp_value;

	// The core vectors:
	vector_table_entry_t reset;
	vector_table_entry_t nmi;
	vector_table_entry_t hard_fault;
	vector_table_entry_t memory_management_fault;
	vector_table_entry_t bus_fault;
	vector_table_entry_t usage_fault;

	RESERVED_WORDS(4);

	vector_table_entry_t supervisor_call;
	vector_table_entry_t debug_monitor;

	RESERVED_WORDS(1);

	vector_table_entry_t pend_sv;
	vector_table_entry_t systick;

	// Interrupt service routine handlers.
	platform_irq_table_t irqs;

} vector_table_t;


/**
 * Global pointer to the system's vector tables.
 */
extern vector_table_t vector_table;


/**
 * Enables the provided register in the system's interrupt controller.
 */
void platform_enable_interrupt(platform_irq_t irq);

/**
 * Enables the provided register in the system's interrupt controller.
 */
void platform_disable_interrupt(platform_irq_t irq);


/**
 * Marks an interrupt as actively pending.
 */
void platform_mark_interrupt_pending(platform_irq_t irq);


/**
 * Marks an interrupt as not currently actively-pending.
 */
void platform_mark_interrupt_serviced(platform_irq_t irq);


/**
 * Marks an interrupt as not currently actively-pending.
 */
bool platform_interrupt_is_pending(platform_irq_t irq);


/**
 * Sets the priority of a given interrupt.
 */
void platform_set_interrupt_priority(platform_irq_t irq, platform_interrupt_priority_t priority);


/**
 * Installs an interrupt handler routine for a given IRQ. Should only be called
 */
void platform_set_interrupt_handler(platform_irq_t irq, interrupt_service_routine_t isr);


#endif
