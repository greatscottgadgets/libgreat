/*
 * This file is part of libgreat
 *
 * LPC43xx misc configuration registers
 */

#ifndef __LPC43XX_PLATFORM_CONFIG__
#define __LPC43XX_PLATFORM_CONFIG__

#include <stddef.h>
#include <assert.h>
#include <stdint.h>

#include <toolchain.h>

/**
 *  LPC43xx misc. configuration registers
 */
typedef volatile struct ATTR_PACKED {
	RESERVED_WORDS(1);

	uint32_t creg0;
	uint32_t creg1;

	RESERVED_WORDS(61);

	uint32_t m4memmap;

	RESERVED_WORDS(5);

	uint32_t creg5;
	uint32_t dmamux;
	uint32_t flashcfga;
	uint32_t flashcfgb;
	uint32_t etbcfg;

	// CREG6
	struct {
		uint32_t ethmode            :  3;
		uint32_t                    :  1;
		uint32_t ctoutctrl          :  1;
		uint32_t                    :  7;
		uint32_t i2s0_tx_sck_in_sel :  1;
		uint32_t i2s0_rx_sck_in_sel :  1;
		uint32_t i2s1_tx_sck_in_sel :  1;
		uint32_t i2s1_rx_sck_in_sel :  1;
		uint32_t emc_clk_sel        :  1;
		uint32_t                    : 15;
	};

	uint32_t m4txevent;

	// TODO: implement the rest of this

} platform_configuration_registers_t;

ASSERT_OFFSET(platform_configuration_registers_t, creg0,    0x004);
ASSERT_OFFSET(platform_configuration_registers_t, m4memmap, 0x100);
ASSERT_OFFSET(platform_configuration_registers_t, etbcfg,   0x128);

/**
 *  ETHMODE constants.
 */
enum {
	ETHMODE_MII = 0,
	ETHMODE_RMII = 4,
};


/**
 * Return a reference to the LPC43xx's CREG block.
 */
platform_configuration_registers_t *get_platform_configuration_registers(void);


/**
 * Remaps the M4 core's address zero to exist in the given region.
 *
 * @param base_addr A pointer to the region to be mapped in.
 */
void platform_remap_address_zero(volatile void *base_addr);


/**
 * @return returns true iff the calling thread is running on the M4
 */
bool platform_running_on_m4(void);


/**
 * @return returns true iff the calling thread is running on the M0
 */
bool platform_running_on_m0(void);


#endif
