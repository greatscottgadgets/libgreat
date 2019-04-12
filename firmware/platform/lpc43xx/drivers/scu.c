/*
 * System Control Unit for the LPC43xx; controls pin multiplexing.
 * Currently a work-in-progress, so unfortunately things still have to rely on the libopencm3 SCU code.
 *
 * This file is part of libgreat
 */

#include <toolchain.h>
#include <drivers/scu.h>

/**
 * @return a reference to the platform's SCU registers
 */
platform_scu_registers_t *platform_get_scu_registers(void)
{
	return (platform_scu_registers_t *)0x40086000;
}


/**
 * Lowest-level API to configure a given pin's configuration in the SCU.
 * Usually, you want one of the platform_scu_configure_pin_<...> functions instead.
 *
 * @param group The SCU group for the pin to be configured. This is the first number, X, in the LPC PX_Y naming scheme.
 * @param pin The SCU pin number for the pin to be configured. This is the second number, Y, in the
 * 		LPC PX_Y naming scheme.
 * @param configuration The configuration to be applied. See platform_scu_pin_configuration_t for more information.
 */
void platform_scu_configure_pin(uint8_t group, uint8_t pin, platform_scu_pin_configuration_t configuration)
{
	platform_scu_registers_t *scu = platform_get_scu_registers();

	// Apply the SCU configuration to the given pin.
	scu->group[group].pin[pin] = configuration;
}

/**
 * Configures a given pin, applying the options that make the most sense for a normal (<30MHz) GPIO.
 *
 * @param group The SCU group for the pin to be configured. This is the first number, X, in the LPC PX_Y naming scheme.
 * @param pin The SCU pin number for the pin to be configured. This is the second number, Y, in the
 * 		LPC PX_Y naming scheme.
 * @param function The function number to set the given pin to GPIO.
 * @param resistors The type of pull resistors to apply.
 */
void platform_scu_configure_pin_gpio(uint8_t group, uint8_t pin, uint8_t function,
	scu_resistor_configuration_t resistors)
{
	// Allow input; keep slew rate and glitch filter on; and apply the relevant pull resistors.
	platform_scu_pin_configuration_t config = {
		.function = function,
		.pull_resistors = resistors,
		.input_buffer_enabled = 1
	};
	platform_scu_configure_pin(group, pin, config);
}

/**
 * Configures a given pin, applying the options that make the most sense for a fast (>30MHz) IO.
 *
 * @param group The SCU group for the pin to be configured. This is the first number, X, in the LPC PX_Y naming scheme.
 * @param pin The SCU pin number for the pin to be configured. This is the second number, Y, in the
 * 		LPC PX_Y naming scheme.
 * @param function The function number to set the given pin to.
 * @param resistors The type of pull resistors to apply.
 */
void platform_scu_configure_pin_fast_io(uint8_t group, uint8_t pin, uint8_t function,
	scu_resistor_configuration_t resistors)
{
	// Allow input; use fast slew, and disable our glitch filter.
	platform_scu_pin_configuration_t config = {
		.function = function,
		.pull_resistors = resistors,
		.input_buffer_enabled = 1,
		.use_fast_slew = 1,
		.disable_glitch_filter = 1,
	};
	platform_scu_configure_pin(group, pin, config);
}


/**
 * Configures a given pin, applying the options that make the most sense for a common UART>
 *
 * @param group The SCU group for the pin to be configured. This is the first number, X, in the LPC PX_Y naming scheme.
 * @param pin The SCU pin number for the pin to be configured. This is the second number, Y, in the
 * 		LPC PX_Y naming scheme.
 * @param function The function number to set the given pin to UART.
 */
void platform_scu_configure_pin_uart(uint8_t group, uint8_t pin, uint8_t function)
{
	platform_scu_configure_pin_gpio(group, pin, function, SCU_NO_PULL);
}
