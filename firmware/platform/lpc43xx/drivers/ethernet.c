/*
 * This file is part of libgreat
 *
 * LPC43xx Ethernet Complex drivers
 */


#include <drivers/ethernet.h>

#include <drivers/platform_reset.h>
#include <drivers/platform_clock.h>


/**
 * @return a reference to the LPC43xx's ethernet registers
 */
static ethernet_register_block_t *get_ethernet_register_block(void)
{
	return (ethernet_register_block_t *)0x40010000;
}


/**
 * Reset the platform's ethernet controller.
 * Should only be called once the ethernet controller is clocked.
 */
static void ethernet_reset_peripheral(void)
{
	platform_reset_register_block_t *reset = get_platform_reset_registers();

	// Issue our reset, and then wait for it to complete.
	reset->ethernet_reset = 1;
	while (reset->ethernet_reset);
}



/**
 * Initialies a new ethernet controller object, and readies it (and the appropriate)
 * hardware for use.
 *
 * @param An unpopulated ethernet device structure to be readied for use.
 */
void platform_ethernet_init(ethernet_controller_t *device)
{
	platform_clock_control_register_block_t *ccu = get_platform_clock_control_registers();

	// Populate the device object with platform-specific knowledge;
	// in this case how to poke ethernet registers.
	device->reg            = get_ethernet_register_block();
	device->platform.creg  = get_platform_configuration_registers();
	device->platform.clock = &ccu->m4.ethernet;

	// Enable clock.
	platform_enable_clock(device->platform.clock, false);

	// Reset the ethernet controller.
	ethernet_reset_peripheral();

	// Figure out PHY clock routing if needed?

	// Switch to RMII.
	// TODO: make this configurable?
	device->platform.creg->ethmode = ETHMODE_RMII;

	// Set up things via the MII link?
	// Set up DMA and MAC modes?
}


void platform_ethernet_configure_phy(ethernet_controller_t *device, uint16_t clock_divider, uint16_t phy_address)
{

}



/**
 * Queue a non-blocking MII transaction, which communicates with the PHY.
 *
 * @param Should be set to true iff the given operation is to be a write.
 * @param register_index The PHY register address.
 * @param value The value to be written to the given PHY register, or 0 for a read operation.
 */
static void platform_ethernet_mii_start_transaction(ethernet_controller_t *device,
		bool is_write, uint8_t register_index, uint16_t value)
{
	// Get a reference to the MII control register.
	platform_ethernet_mii_address_register_t *addr = &device->reg->mac.mii_addr;

	// Wait for the MII bus to be ready.
	while (addr->comms_in_progress);

	// Populate the index of the target MII register.
	addr->register_index = register_index;

	// If this is a write, mark it as a write, and queue the data to be transmitted.
	if (is_write) {
		device->reg->mac.mii_data = value;
		addr->write = 1;
	} else {
		addr->write = 0;
	}

	// Finally, trigger the transaction.
	addr->comms_in_progress = 1;
}


/**
 * Queue a non-blocking MII write, which communicates with the PHY over the
 * management interface. To emulate a blocking write, follow this up by calling
 * for platform_ethernet_mii_complete_transaction().
 *
 * @param register_index The PHY register address.
 * @param value The value to be written to the given PHY register.
 */
void platform_ethernet_mii_write(ethernet_controller_t *device, uint8_t register_index, uint16_t value)
{
	platform_ethernet_mii_start_transaction(device, true, register_index, value);
}


/**
 * @return true iff a management read or write is currently in progress
 */
bool platform_ethernet_mii_write_in_progress(ethernet_controller_t *device)
{
	return device->reg->mac.mii_addr.comms_in_progress;
}


/**
 * Blocks until the active management transaction completes.
 *
 * @returns The relevant MII data; mostly useful to retrieve the result
 *		of a completed read.
 */
uint16_t platform_ethernet_mii_complete_transaction(ethernet_controller_t *device)
{
	// Wait for the transaction to complete.
	while (platform_ethernet_mii_write_in_progress(device));

	// ... and return the last bit of data.
	return device->reg->mac.mii_data;
}


/**
 * Queue a non-blocking MII read, which communicates with the PHY over the
 * management interface. The result of this read can be read by calling
 * platform_ethernet_mii_complete_transcation(); its readiness can be checked using
 * platform_ethernet_mii_write_in_progress.
 *
 * @param register_index The PHY register address.
 * @param value The value to be written to the given PHY register.
 */
void platform_ethernet_mii_start_read(ethernet_controller_t *device, uint8_t register_index)
{
	// Start an MII read transaction...
	platform_ethernet_mii_start_transaction(device, false, register_index, 0);
}



/**
 * Blocking read from the PHY over the management interface.
 *
 * @param register_index The register to read from.
 * @return The result of the read operation.
 */
uint16_t platform_ethernet_mii_read(ethernet_controller_t *device, uint8_t register_index)
{
	// Start an MII read transaction...
	platform_ethernet_mii_start_read(device, register_index);

	// ... and wait for it to complete, and then return the result.
	return platform_ethernet_mii_complete_transaction(device);
}
