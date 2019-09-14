/*
 * System Control Unit for the LPC43xx; controls pin multiplexing.
 * Currently a work-in-progress, so unfortunately things still have to rely on the libopencm3 SCU code.
 *
 * This file is part of libgreat
 */


#ifndef __LIBGREAT_PLATFORM_SCU__
#define __LIBGREAT_PLATFORM_SCU__

#include <toolchain.h>

// The size of an SCU block, in bytes.
#define SCU_LPC_GROUP_BLOCK_SIZE (32 * sizeof(uint32_t))

// The base address of the SCU register bank.
#define SCU_BASE                 (0x40086000UL)


/**
 * Enumeration specifying the type of internal resistors to be connected to a given pin.
 */
typedef enum {
	SCU_NO_PULL    = 0b10,
	SCU_PULLDOWN   = 0b11,
	SCU_PULLUP     = 0b00,
	SCU_REPEATER   = 0b01,

	// Platform agnostic names.
	RESISTOR_CONFIG_NO_PULL    = 0b10,
	RESISTOR_CONFIG_PULLDOWN   = 0b11,
	RESISTOR_CONFIG_PULLUP     = 0b00,
	RESISTOR_CONFIG_KEEPER     = 0b01,
 } scu_resistor_configuration_t;


/**
 * Type that represents an SCU register for a given pin.
 */
typedef struct ATTR_PACKED {
	uint32_t function              : 3;
	uint32_t pull_resistors        : 2;
	uint32_t use_fast_slew         : 1;
	uint32_t input_buffer_enabled  : 1;
	uint32_t disable_glitch_filter : 1;
	uint32_t                       : 24;
} platform_scu_pin_configuration_t;
typedef volatile platform_scu_pin_configuration_t platform_scu_pin_register_t;


/**
 * Type that represents a SCU register group.
 */
typedef struct ATTR_PACKED {

	// Each group consists of 32 pin registers.
	// Not all pins may be implemented for each group; see the LPC43xx datasheet.
	platform_scu_pin_register_t pin[32];

} platform_scu_pin_group_registers_t;


/**
 * Type that represents the register layout fo the LPC43xx System Control Unit (SCU).
 */
typedef volatile struct ATTR_PACKED {

	// Special function select (SFS) registers for each of the multiplexed pins on the GPIO.
	platform_scu_pin_group_registers_t group[16];

	RESERVED_WORDS(256);

	// Special function select (SFS) register for the clock pins.
	platform_scu_pin_configuration_t clk[4];

	// TODO: add the registers for the USB/ADC/EMC/SD/INT pins

} platform_scu_registers_t;


/**
 * Collection that contains sets of {SCU group, SCU pin, SCU function}; used
 * so subordinate drivers and code can specify lists of utilized pin mappings.
 */
 typedef struct {
	 uint8_t group;
	 uint8_t pin;
	 uint8_t function;
} scu_function_mapping_t;


/**
 * @return a reference to the platform's SCU registers
 */
platform_scu_registers_t *platform_get_scu_registers(void);


/**
 * Lowest-level API to configure a given pin's configuration in the SCU.
 * Usually, you want one of the platform_scu_configure_pin_<...> functions instead.
 *
 * @param group The SCU group for the pin to be configured. This is the first number, X, in the LPC PX_Y naming scheme.
 * @param pin The SCU pin number for the pin to be configured. This is the second number, Y, in the
 * 		LPC PX_Y naming scheme.
 * @param configuration The configuration to be applied. See platform_scu_pin_configuration_t for more information.
 */
void platform_scu_configure_pin(uint8_t group, uint8_t pin, platform_scu_pin_configuration_t configuration);


/**
 * Convenience variant of `platform_scu_configure_pin` that accepts its group, pin, and function
 * from a scu_function_mapping_t object.
 *
 * @param mapping A mapping object containing the port, pin, and function to be applied.
 * @param configuration The configuration to be applied -- its function field is ignored, as the
 *     mapping field provides that.
 */
void platform_scu_apply_mapping(scu_function_mapping_t mapping, platform_scu_pin_configuration_t configuration);


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
	scu_resistor_configuration_t resistors);


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
	scu_resistor_configuration_t resistors);


/**
 * Configures a given pin, applying the options that make the most sense for a common UART.
 *
 * @param group The SCU group for the pin to be configured. This is the first number, X, in the LPC PX_Y naming scheme.
 * @param pin The SCU pin number for the pin to be configured. This is the second number, Y, in the
 * 		LPC PX_Y naming scheme.
 * @param function The function number to set the given pin to UART.
 */
void platform_scu_configure_pin_uart(uint8_t group, uint8_t pin, uint8_t function);





#endif
