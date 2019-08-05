/*
 * This file is part of libgreat
 *
 * LPC43xx GPIO functions
 */

#include <toolchain.h>
#include <drivers/scu.h>



#ifndef __LIBGREAT_PLATFORM_GPIO_H__
#define __LIBGREAT_PLATFORM_GPIO_H__


// For LPCxx devices, use the SCU resistor configuration as the GPIO resistor configuration.
typedef scu_resistor_configuration_t gpio_resistor_configuration_t;


// Describe the chip's GPIO capabilities.
#define GPIO_MAX_PORTS 6
#define GPIO_MAX_PORT_BITS 20

/**
 * Simple pair of identifiers for a GPIO pin.
 */
typedef struct {
	uint8_t port;
	uint8_t pin;
} gpio_pin_t;


/**
 * Convenience function that converts a port and pin number into a gpio_pin_t.
 */
static inline gpio_pin_t gpio_pin(uint8_t port, uint8_t pin)
{
	gpio_pin_t converted = { port, pin };
	return converted;
}


/**
 * Returns the SCU group number for the given GPIO bit.
 */
uint8_t gpio_get_group_number(gpio_pin_t pin);

/**
 * Returns the SCU pin number for the given GPIO bit.
 */
uint8_t gpio_get_pin_number(gpio_pin_t pin);


/**
 * LPC43xx specicfic register that grabs a GPIO pin word-access register.
 *
 * @returns a register that always contains -1 (all 1's) if the bit is high, or 0 if the bit is low
 */
uint32_t *platform_gpio_get_pin_register(gpio_pin_t pin);



#endif // __LIBGREAT_GPIO_H__
