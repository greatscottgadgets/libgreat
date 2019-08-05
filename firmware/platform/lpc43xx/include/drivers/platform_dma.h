/**
 * Drivers for the LPC43xx third-party ("general purpose") DMA controller.
 *
 * This file is part of libgreat.
 */

 #ifndef __LIBGREAT_PLATFORM_DRIVER_DMA__
 #define __LIBGREAT_PLATFORM_DRIVER_DMA__

 #include <toolchain.h>


/**
 *  LPC43xx GPDMA configuration registers.
 */
typedef volatile struct ATTR_PACKED {

	uint32_t interrupt_status;
	uint32_t terminal_count_interrupt_status;
	uint32_t terminal_count_interrupt_clear;
	uint32_t error_interrupt_status;
	uint32_t error_interrupt_clear;
	uint32_t raw_terminal_count_status_register;
	uint32_t raw_error_interrupt_status_register;

	uint32_t enabled_channel_bitmask;

	// DMA request trigger registers.
	uint32_t software_burst_request_bitmask;
	uint32_t software_single_request_bitmask;
	uint32_t software_last_burst_request_bitmask;
	uint32_t software_last_single_request_bitmask;

	// Configuration register.
	struct {
		uint32_t dma_controller_enabled   :  1;
		uint32_t ahb0_use_big_endian_mode :  1;
		uint32_t ahb1_use_big_endian_mode :  1;
		uint32_t ahb1_use_big_endian_mode : 29;
	};


	uint32_t request_synchronization_bitmask;


};


 #endif
