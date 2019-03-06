/*
 * This file is part of GreatFET
 *
 * Code for controlling an AD970x DAC.
 */

#ifndef __LIBGREAT_AD970X_H__
#define __LIBGREAT_AD970X_H__

#include <toolchain.h>
#include <drivers/gpio.h>

/**
 * Structure representing an AD970X DAC.
 */
typedef struct {

	// GPIO port locations for each of the given GPIO pins.
	uint8_t gpio_port_cs;
	uint8_t gpio_port_sck;
	uint8_t gpio_port_data;
	uint8_t gpio_port_mode;

	// GPIO pin locations for each of the given GPIO pins.
	uint8_t gpio_pin_cs;
	uint8_t gpio_pin_sck;
	uint8_t gpio_pin_data;
	uint8_t gpio_pin_mode;

	// Internal fields:

	// The length of a half-period of the DAC configuration clock.
	uint32_t config_half_period;

} ad970x_t;


/**
 * Sets up a new connection to an AD970x DAC.
 *
 * @param dac The DAC object to be initialized. Must already have its gpio_port_* and gpio_pin_* properties initialized
 * 		to the correct GPIO ports/pins for the DAC.
 * @param clock_period The (very approximate) clock period, in microseconds. Must be divisible by two.
 *
 * @return 0 on success, or an error code on failure
 */
int ad970x_initialize(ad970x_t *dac, uint32_t clock_period);


/**
 * Reads a DAC configuration register.
 *
 * @param address The register address to touch.
 * @return The raw value read.
 */
uint8_t ad970x_register_read(ad970x_t *dac, uint8_t address);

/**
 * Writes a DAC configuration register.
 *
 * @param address The register address to touch.
 * @param value The raw value to be written.
 */
void ad970x_register_write(ad970x_t *dac, uint8_t address, uint8_t value);

#endif
