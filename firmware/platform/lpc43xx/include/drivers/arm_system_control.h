/*
 * This file is part of libgreat.
 *
 * ARM system control drivers.
 */

#ifndef __ARM_SYSTEM_CONTROL__
#define __ARM_SYSTEM_CONTROL__

#include <toolchain.h>

typedef volatile struct {

	const uint32_t cpuid;

	// Interrupt control and status register
	uint32_t icsr;

	// Vector table offset register
	uint32_t vtor;

	// Application interrupt and reset control register
	uint32_t aircr;

	// System control register
	uint32_t scr;

	// Configuration control register
	uint32_t ccr;

	// System Handler Priority registers
	uint32_t shpr[3];

	// System Handler control/status registers
	uint32_t shcsr;

	// Configurable fault status register
	union {
		uint32_t cfsr;
		struct {
			uint16_t ufsr;
			uint8_t  bfsr;
			uint8_t mmfsr;
		} __attribute__((packed));
	};

	// Hard fault status register.
	uint32_t hfsr;

	// Debug fault status register.
	uint32_t dfsr;

	// Memory Management fault address register
	uint32_t mmfar;

	// Bus Fault address register
	uint32_t bfar;

	// Aux Fault status register
	uint32_t afsr;

	// Processor feature register
	const uint32_t pfr[1];

	// Debug feature register.
	const uint32_t dfr;

	// Aux feature register
	const uint32_t afr;

	// Memory Model feature registert
	const uint32_t mmfr[4];

	// Instruction set attributes register.
	const uint32_t isar[5];

	RESERVED_WORDS(5);

	// Coprocessor access control register.
	// Note that the FPU is actually split between two "co-processors",
	// so there's actually two two-bit registers that must be set identically.,
	// We treat these as a single four-bit register.
	struct {
		uint32_t            : 20;
		uint32_t fpu_access :  4;
		uint32_t            :  8;
	} cpacr;

} ATTR_PACKED arm_system_control_register_block_t;

ASSERT_OFFSET(arm_system_control_register_block_t, afsr, 0x3c);


/**
 * Constants for the CPACR fpu_access bits.
 */
typedef enum {
	FPU_DISABLED        = 0b0000,
	FPU_PRIVILEGED_ONLY = 0b0101,
	FPU_FULL_ACCESS     = 0b1111,
} fpu_access_rights_t;


/**
 * @return a reference to the ARM SCB.
 */
arm_system_control_register_block_t *arch_get_system_control_registers(void);


/**
 * Enables access to the system's FPU.
 *
 * @param allow_unprivileged_access True iff user-mode should be able to use the FPU.
 */
void arch_enable_fpu(bool allow_unprivileged_access);

#endif
