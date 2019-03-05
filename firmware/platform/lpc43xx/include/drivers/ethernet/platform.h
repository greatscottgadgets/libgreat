/*
 * This file is part of libgreat
 *
 * LPC43xx Ethernet Complex data structures
 */

#ifndef __LIBGREAT_ETHERNET_PLATFORM_H__
#define __LIBGREAT_ETHERNET_PLATFORM_H__

#include <stdint.h>
#include <stddef.h>

#include <toolchain.h>
#include <drivers/platform_clock.h>
#include <drivers/platform_reset.h>
#include <drivers/platform_config.h>
#include <drivers/ethernet/platform.h>


// Opaque references to ethernet controller objects.
typedef struct ethernet_controller ethernet_controller_t;


/**
 * Ethernet clock divider frequencies.
 */
enum {
	CSR_DIV_BY_42  = 0,
	CSR_DIV_BY_62  = 1,
	CSR_DIV_BY_16  = 2,
	CSR_DIV_BY_26  = 3,
	CSR_DIV_BY_102 = 4,
	CSR_DIV_BY_124 = 5,
};



/**
 * Structure representing a MII configuration register.
 */
typedef volatile struct ATTR_PACKED {
	uint32_t comms_in_progress :  1;
	uint32_t write             :  1;
	uint32_t csr_clock_range   :  4;
	uint32_t register_index    :  5;
	uint32_t phy_address       :  5;
	uint32_t                   : 16;
} platform_ethernet_mii_address_register_t;


/**
 * Structure representing the LPC43xx MAC configuration registers.
 */
typedef struct ATTR_PACKED {
	uint32_t config;
	uint32_t frame_filter;
	uint64_t hashtable;

	/**
	 * Structure describing how management data should be addressed.
	 */
	platform_ethernet_mii_address_register_t mii_addr;
	struct {
		uint32_t mii_data : 16;
		uint32_t  : 16;
	};

	uint32_t flow_ctrl;
	uint32_t vlan_tag;
	uint32_t _reserved0;
	uint32_t debug;
	uint32_t rwake_frflt;
	uint32_t pmt_ctrl_stat;

	RESERVED_WORDS(2);

	uint32_t intr;
	uint32_t intr_mask;
	uint64_t addr0;
} ethernet_mac_register_block_t;

ASSERT_OFFSET(ethernet_mac_register_block_t, intr, 0x38);

/**
 * Structure representing the LPC43xx DMA configuration registers.
 */
typedef struct ATTR_PACKED {
	uint32_t bus_mode;
	uint32_t trans_poll_demand;
	uint32_t rec_poll_demand;
	uint32_t rec_des_addr;
	uint32_t trans_des_addr;
	uint32_t stat;
	uint32_t op_mode;
	uint32_t int_en;
	uint32_t mfrm_bufof;
	uint32_t rec_int_wdt;

	RESERVED_WORDS(8);

	uint32_t curhost_trans_des;
	uint32_t curhost_rec_des;
	uint32_t curhost_trans_buf;
	uint32_t curhost_rec_buf;

} ethernet_dma_register_block_t;

ASSERT_OFFSET(ethernet_dma_register_block_t, curhost_trans_des, 0x48);


/**
 *  Structure representing the LPC43xx ethernet register block.
 */
typedef volatile struct ATTR_PACKED {

	// MAC control registers.
	ethernet_mac_register_block_t mac;

	RESERVED_WORDS(431);

	// Time registers.
	uint32_t subsecond_incr;
	uint32_t seconds;
	uint32_t nanoseconds;
	uint32_t secondsupdate;
	uint32_t nanosecondsupdate;
	uint32_t addend;
	uint32_t targetseconds;
	uint32_t targetnanoseconds;
	uint32_t highword;
	uint32_t timestampstat;

	RESERVED_WORDS(565);

	// DMA configuration registers.
	ethernet_dma_register_block_t dma;

} ethernet_register_block_t;


ASSERT_OFFSET(ethernet_register_block_t, subsecond_incr, 0x0704);
ASSERT_OFFSET(ethernet_register_block_t, dma,            0x1000);


/**
 * Platform-specific data for ethernet drivers.
 */
typedef struct {

	// Reference to the system's platform configuration registers.
	platform_configuration_registers_t *creg;

	// Pointer to the clock that's used for the current controller.
	platform_branch_clock_register_t *clock;

} ethernet_platform_data_t;


/**
 * Initialies a new ethernet controller object, and readies it (and the appropriate)
 * hardware for use.
 *
 * @param An unpopulated ethernet device structure to be readied for use.
 */
void platform_ethernet_init(ethernet_controller_t *device);



/**
 * Queue a non-blocking MII transaction, which communicates with the PHY.
 *
 * @param Should be set to true iff the given operation is to be a write.
 * @param register_index The PHY register address.
 * @param value The value to be written to the given PHY register, or 0 for a read operation.
 */
static void platform_ethernet_mii_start_transaction(ethernet_controller_t *device,
		bool is_write, uint8_t register_index, uint16_t value);


/**
 * Queue a non-blocking MII write, which communicates with the PHY over the
 * management interface. To emulate a blocking write, follow this up by calling
 * for platform_ethernet_mii_complete_transaction().
 *
 * @param register_index The PHY register address.
 * @param value The value to be written to the given PHY register.
 */
void platform_ethernet_mii_write(ethernet_controller_t *device, uint8_t register_index, uint16_t value);


/**
 * @return true iff a management read or write is currently in progress
 */
bool platform_ethernet_mii_write_in_progress(ethernet_controller_t *device);

/**
 * Blocks until the active management transaction completes.
 *
 * @returns The relevant MII data; mostly useful to retrieve the result
 *		of a completed read.
 */
uint16_t platform_ethernet_mii_complete_transaction(ethernet_controller_t *device);


/**
 * Queue a non-blocking MII read, which communicates with the PHY over the
 * management interface. The result of this read can be read by calling
 * platform_ethernet_mii_complete_transcation(); its readiness can be checked using
 * platform_ethernet_mii_write_in_progress.
 *
 * @param register_index The PHY register address.
 * @param value The value to be written to the given PHY register.
 */
void platform_ethernet_mii_start_read(ethernet_controller_t *device, uint8_t register_index);


/**
 * Blocking read from the PHY over the management interface.
 *
 * @param register_index The register to read from.
 * @return The result of the read operation.
 */
uint16_t platform_ethernet_mii_read(ethernet_controller_t *device, uint8_t register_index);


#endif
