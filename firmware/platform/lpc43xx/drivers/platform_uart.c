/*
 * This file is part of libgreat
 *
 * LPC43xx-specific UART code.
 */

#include <debug.h>

#include <drivers/uart.h>
#include <drivers/platform_clock.h>
#include <drivers/scu.h>

typedef struct {

	// Identifies the UART's location in the SCU.
	uint8_t group;
	uint8_t pin;

	// The SCU function number that switches a given pin to UART mode.
	uint8_t function;

} uart_pin_t;

typedef struct {

	// Transmit / receive.
	uart_pin_t tx;
	uart_pin_t rx;

	uart_pin_t rts;
	uart_pin_t cts;


} uart_pins_t;


/**
 * Mapping that provides the location of the default pins for each UART.
 */
uart_pins_t uart_pins[] = {
	{
		.tx = { .group = 9, .pin = 5, .function = 7 },
		.rx = { .group = 9, .pin = 6, .function = 7 }

		// TODO: add flow control pins!
	},

	// FIXME: add the other UARTS!
};





/**
 * @return the register bank for the given UART
 */
static platform_uart_registers_t *get_uart_registers(uart_number_t uart_number)
{
	switch(uart_number) {
		case 0: return (platform_uart_registers_t *)0x40081000;
		case 1: return (platform_uart_registers_t *)0x40082000;
		case 2: return (platform_uart_registers_t *)0x400C1000;
		case 3: return (platform_uart_registers_t *)0x400C2000;
	}

	pr_error("uart: tried to set up a non-existant UART %d!\n", uart_number);
	return 0;
}



static platform_branch_clock_t *get_clock_for_uart(uart_number_t number)
{
	platform_clock_control_register_block_t *ccu = get_platform_clock_control_registers();

	switch(number)  {
		case UART0: return &ccu->usart0;
		case UART1: return &ccu->uart1;
		case UART2: return &ccu->usart2;
		case UART3: return &ccu->usart3;
	}

	pr_error("cannot find a clock for UART %d!\n", number);
	return NULL;
}


/**
 * Performs platform-specific initialization for the given UART.
 */
int platform_uart_init(uart_t *uart)
{
	uart_pins_t pins = uart_pins[uart->number];

	// Fetch the registers for the relevant UART.
	uart->reg = get_uart_registers(uart->number);
	if (!uart->reg) {
		return EINVAL;
	}

	// Figure out the clock that drives the relevant UART...
	uart->platform_data.clock = get_clock_for_uart(uart->number);
	if (!uart->platform_data.clock) {
		return EINVAL;
	}

	// ... and ensure that it's on.
	platform_enable_branch_clock(uart->platform_data.clock, false);

	// Connect our UART pins to the UART hardware.
	platform_scu_configure_pin_uart(pins.tx.group, pins.tx.pin, pins.tx.function);
	platform_scu_configure_pin_uart(pins.rx.group, pins.rx.pin, pins.rx.function);

	return 0;
}


/**
 * @return the frequency of the clock driving this UART, in Hz
 */
uint32_t platform_uart_get_parent_clock_frequency(uart_t *uart)
{
	return platform_get_branch_clock_frequency(uart->platform_data.clock);
}
