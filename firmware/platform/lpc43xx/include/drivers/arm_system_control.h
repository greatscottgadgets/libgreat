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
	union {
		uint32_t shcsr;
		struct {
			uint32_t memory_management_fault_active   :  1;
			uint32_t bus_fault_active                 :  1;
			uint32_t                                  :  1;
			uint32_t usage_fault_active               :  1;
			uint32_t                                  :  3;
			uint32_t svc_enabled                      :  1;
			uint32_t debug_monitor_active             :  1;
			uint32_t                                  :  1;
			uint32_t pended_svc_enabled               :  1;
			uint32_t systick_enabled                  :  1;
			uint32_t usage_fault_pending              :  1;
			uint32_t memory_management_fault_pending  :  1;
			uint32_t bus_fault_pending                :  1;
			uint32_t svc_pending                      :  1;
			uint32_t memory_managemnt_faults_enabled  :  1;
			uint32_t bus_faults_enabled               :  1;
			uint32_t usage_faults_enabled             :  1;
			uint32_t                                  : 13;
		};
	};

	// Configurable fault status register
	union {
		uint32_t cfsr;
		struct {

			// Memory management fault status register
			union {
				uint8_t mmfsr;
				struct {
					uint8_t instruction_access     : 1;
					uint8_t data_access            : 1;
					uint8_t                        : 1;
					uint8_t on_unstacking          : 1;
					uint8_t on_stacking            : 1;
					uint8_t fp_state_saving        : 1;
					uint8_t                        : 1;
					uint8_t fault_address_valid    : 1;
				} memory_management_faults;
			};


			// Bus fault status register
			union {
				uint8_t  bfsr;
				struct {
					uint8_t instruction_bus     : 1;
					uint8_t precise_data_bus    : 1;
					uint8_t imprecise_data_bus  : 1;
					uint8_t on_unstacking       : 1;
					uint8_t on_stacking         : 1;
					uint8_t fp_state_saving     : 1;
					uint8_t                     : 1;
					uint8_t fault_address_valid : 1;

				} bus_faults;
			};


			// Usage fault status register
			union {
				uint16_t ufsr;

				struct {
					uint16_t undefined_instruction : 1;
					uint16_t invalid_state         : 1;
					uint16_t invalid_pc            : 1;
					uint16_t no_coprocessor        : 1;
					uint16_t                       : 4;
					uint16_t unaligned_access      : 1;
					uint16_t divide_by_zero        : 1;
					uint16_t                       : 6;
				} usage_faults;
			};

		} __attribute__((packed));
	};

	// Hard fault status register.
	union {
		uint32_t hfsr;
		struct {
			uint32_t                      :  1;
			uint32_t on_vector_table_read :  1;
			uint32_t                      : 28;
			uint32_t forced               :  1;
			uint32_t debug_event          :  1;
		} hard_faults;
	};

	// Debug fault status register.
	uint32_t dfsr;

	// Memory Management fault address register
	union {
		uint32_t mmfar;
		uint32_t memory_management_faulting_address;
	};

	// Bus Fault address register
	union {
		uint32_t bfar;
		uint32_t bus_faulting_address;
	};

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

	RESERVED_WORDS(6);

	// Coprocessor access control register.
	// Note that the FPU is actually split between two "co-processors",
	// so there's actually two two-bit registers that must be set identically.,
	// We treat these as a single four-bit register.
	union {
		struct {
			uint32_t            : 20;
			uint32_t fpu_access :  4;
			uint32_t            :  8;
		} cpacr;
		uint32_t cpacr_raw;
	};

} ATTR_PACKED arm_system_control_register_block_t;

ASSERT_OFFSET(arm_system_control_register_block_t, afsr,  0x3c);
ASSERT_OFFSET(arm_system_control_register_block_t, cpacr, 0x88);


/**
 * Constants for the CPACR fpu_access bits.
 */
typedef enum {
	FPU_DISABLED        = 0b0000,
	FPU_PRIVILEGED_ONLY = 0b0101,
	FPU_FULL_ACCESS     = 0b111,
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
