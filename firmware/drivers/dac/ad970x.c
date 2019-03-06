/*
 * This file is part of GreatFET
 *
 * Code for controlling an AD970x DAC.
 */

#include <debug.h>

#include <drivers/timer.h>
#include <drivers/dac/ad970x.h>

/**
 * Configuration constants for the AD970x protocol.
 */
enum {

	// DAC command -- direction bit.
	// Identifies if the command to follow is a READ or a WRITE.
	DAC_DIRECTION_READ  = (1 << 7),
	DAC_DIRECTION_WRITE = (0 << 7),

	// DAC command -- length.
	// Identifies the length of the data stage of this command.
	DAC_WIDTH_BYTE = (0 << 5)

};


/**
 * Sets up a new connection to an AD970x DAC.
 *
 * @param dac The DAC object to be initialized. Must already have its gpio_port_* and gpio_pin_* properties initialized
 * 		to the correct GPIO ports/pins for the DAC.
 * @param clock_period The (approximate) clock period, in microseconds. Must be divisible by two. Can be set to 0 to
		iterate as fast as our bit-banging allows.
 *
 * @return 0 on success, or an error code on failure
 */
int ad970x_initialize(ad970x_t *dac, uint32_t clock_period)
{
	// Store our half period.
	dac->config_half_period = clock_period / 2;

	// Validate our period.
	if (clock_period && !dac->config_half_period) {
		pr_error("error: tried to configure DAC for a currently-impossible period < 2uS!\n");
		return EINVAL;
	}

	// Set the SCK and CS pins to output only, as we'll maintain control over these.
	gpio_set_pin_direction(dac->gpio_port_cs,  dac->gpio_pin_cs, true);
	gpio_set_pin_direction(dac->gpio_port_sck, dac->gpio_pin_sck, true);

	// We'll start by driving the data line; and release it when we need to.
	gpio_set_pin_direction(dac->gpio_port_data, dac->gpio_pin_data, true);

	// Keep the DAC in SPI mode, for now.
	gpio_set_pin_direction(dac->gpio_port_mode, dac->gpio_pin_mode, true);
	gpio_clear_pin(dac->gpio_port_mode, dac->gpio_pin_mode);

	return 0;
}

/**
 * Convenience function to set SCK.
 */
static void dac_set_sck_high(ad970x_t *dac)
{
	gpio_set_pin(dac->gpio_port_sck, dac->gpio_pin_sck);
}

/**
 * Convenience function to clear SCK.
 */
static void dac_set_sck_low(ad970x_t *dac)
{
	gpio_clear_pin(dac->gpio_port_sck, dac->gpio_pin_sck);
}

/**
 * Convenience function to read the DATA line.
 */
static uint8_t dac_read_data_state(ad970x_t *dac)
{
	return gpio_get_pin_value(dac->gpio_port_data, dac->gpio_pin_data);
}

/**
 * Convenience function to read the DATA line.
 */
static void dac_set_data_state(ad970x_t *dac, uint8_t value)
{
	gpio_set_pin_value(dac->gpio_port_data, dac->gpio_pin_data, value);
}

/**
 * Convenience function that waits for a half of the DAC configuraiton period.
 */
static void dac_wait_for_half_period(ad970x_t *dac)
{
	if (dac->config_half_period) {
		delay_us(dac->config_half_period);
	}
}

/**
 * Begins driving the DAC configuration data line, taking control of the bus.
 */
static void dac_drive_data_line(ad970x_t *dac)
{
	gpio_set_pin_direction(dac->gpio_port_data, dac->gpio_pin_data, true);
	dac_wait_for_half_period(dac);
}


/**
 * Cease driving the DAC configuration data line, allowing the DAC to respond.
 */
static void dac_release_data_line(ad970x_t *dac)
{
	gpio_set_pin_direction(dac->gpio_port_data, dac->gpio_pin_data, false);
	dac_wait_for_half_period(dac);
}



/**
 * Reads a single bit from the AD970x configuration bus.
 * Synchronously blocks for the bit period.
 */
static uint8_t dac_receive_bit(ad970x_t *dac)
{
	uint8_t bit;

	// Drive the clock low for half of a period.
	dac_set_sck_low(dac);
	dac_wait_for_half_period(dac);

	// Sample the DATA pin's value _just before_ we'll go high.
	// This gives the value the most time to settle, as it's set by the DAC on the falling edge.
	bit = dac_read_data_state(dac);

	// Drive the clock high for the remainder of the period.
	dac_set_sck_high(dac);
	dac_wait_for_half_period(dac);

	// Return the value we observed.
	return bit;
}

/**
 * Writes a single bit to the AD970x configuration bus.
 * Synchronously blocks for the bit period.
 */
static void dac_send_bit(ad970x_t *dac, uint8_t value)
{
	// Drive DATA to a given value...
	dac_set_data_state(dac, value);

	//... and then re-use our "read bit" code to iterate through the cycle.
	dac_receive_bit(dac);
}

/**
 * Starts a DAC configuration transaction.
 */
static void dac_start_config_transaction(ad970x_t *dac)
{
	// Clear CS, and wait a bit to meet timing requirements.
	gpio_clear_pin(dac->gpio_port_cs, dac->gpio_pin_cs);
	dac_wait_for_half_period(dac);
}


/**
 * Termiantes a configuration transaction, placing the configruation bus back into its idle state.
 */
static void dac_end_config_transaction(ad970x_t *dac)
{
	// Put the system back into its idle state (CS high, SCK low).
	gpio_set_pin(dac->gpio_port_cs, dac->gpio_pin_cs);
	dac_set_sck_low(dac);

	// Block for half a period to meet timing requirements.
	dac_wait_for_half_period(dac);
}


/**
 * Writes a single byte onto the DAC configuration interface.

 * @param value The value to write.
 */
static void dac_send_byte(ad970x_t *dac, uint8_t value)
{
	// Set the SDIO port to output mode, so we can issue our write.
	dac_drive_data_line(dac);

	// Transmit each of the bits.
	for (int i = 7; i >= 0; --i) {

		// Determine the bit to be scanned out.
		uint8_t bit = value & (1 << i);

		// ... and scan it out.
		dac_send_bit(dac, bit);
	}
}

/**
 * Reads a single byte from the DAC configuration interface.
 *
 * @return the byte read from the interface
 */
static uint8_t dac_receive_byte(ad970x_t *dac)
{
	uint8_t byte = 0;

	// Set the SDIO port to input mode, so we can get the response.
	dac_release_data_line(dac);

	// ... and read each of the bits.
	for (int i = 0; i < 8; ++i) {

		// Read the current bit...
		uint8_t bit = dac_receive_bit(dac);

		// .. and melt it into our running shift.
		byte = (byte << 1) | bit;
	}

	return byte;
}

/**
 * Reads a DAC configuration register.
 *
 * @param address The register address to touch.
 * @return The raw value read.
 */
uint8_t ad970x_register_read(ad970x_t *dac, uint8_t address)
{
	uint8_t command = DAC_DIRECTION_READ | DAC_WIDTH_BYTE | address;
	uint8_t response;

	dac_start_config_transaction(dac);

	// Scan out the command, and then read back the response.
	dac_send_byte(dac, command);
	response = dac_receive_byte(dac);

	dac_end_config_transaction(dac);
	return response;
}

/**
 * Writes a DAC configuration register.
 *
 * @param address The register address to touch.
 * @param value The raw value to be written.
 */
void ad970x_register_write(ad970x_t *dac, uint8_t address, uint8_t value)
{
	uint8_t command = DAC_DIRECTION_WRITE | DAC_WIDTH_BYTE | address;

	dac_start_config_transaction(dac);

	// Scan out the command, and then scan out the argument.
	dac_send_byte(dac, command);
	dac_send_byte(dac, value);

	dac_end_config_transaction(dac);
}
