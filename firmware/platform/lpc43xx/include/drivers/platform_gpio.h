/*
 * This file is part of libgreat
 *
 * LPC43xx GPIO functions
 */

#include <toolchain.h>


#ifndef __LIBGREAT_PLATFORM_GPIO_H__
#define __LIBGREAT_PLATFORM_GPIO_H__

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

#endif // __LIBGREAT_GPIO_H__
