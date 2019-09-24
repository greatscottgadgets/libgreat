/*
 * This file is part of libgreat
 *
 * LPC43xx DAC functions
 */


#ifndef __LIBGREAT_PLATFORM_DAC_H__
#define __LIBGREAT_PLATFORM_DAC_H__

#include <toolchain.h>


typedef struct dac dac_t;


/**
 * Structure representing the in-memory layout of a DAC peripheral.
 */
typedef volatile struct ATTR_PACKED platform_dac_registers {

	// Conversion register.
	// Includes the digital value to be converted to an analog output value and
	// a bit that trades off performance versus power.
	union {
		struct {
			uint32_t /*reserved*/                     :  6;
			uint32_t conversion_value                 : 10;

			// If set, the update-rate maximum is 400 kHz,
			// with shorter settling times and lower power consumption.
			// If unset, the update-rate maximum is 1 MHz,
			// with longer settling times and higher power consumption.
			uint32_t bias_settle_time_for_low_power   :  1;

			uint32_t /*reserved*/                     : 15;
		};
		uint32_t conversion_register;
	};

	// Control register.
	// Enables the DMA operation and controls the DMA timer.
	union {
		struct {
			uint32_t dma_request        :  1;
			uint32_t dma_double_buffer  :  1;
			uint32_t dma_timeout        :  1;
			uint32_t dma_and_dac_enable :  1;
			uint32_t /*reserved*/       : 28;
		};
		uint32_t control_register;
	};

	// Counter value register.
	// Contains the reload value for the Interrupt/DMA counter.
	union {
		struct {
			uint32_t counter_value : 16;
			uint32_t /*reserved*/  : 16;
		};
		uint32_t counter_value_register;
	};

} platform_dac_registers_t;

ASSERT_OFFSET(platform_dac_registers_t, control_register, 0x04);
ASSERT_OFFSET(platform_dac_registers_t, counter_value_register, 0x08);

platform_dac_registers_t *platform_get_dac_registers();
int platform_dac_init(dac_t *dac);
void dac_set_value(dac_t *dac, uint32_t value);

#endif
