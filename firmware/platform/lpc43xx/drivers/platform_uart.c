/*
 * This file is part of libgreat
 *
 * LPC43xx-specific UART code.
 */

#include <debug.h>

#include <drivers/scu.h>
#include <drivers/uart.h>
#include <drivers/arm_vectors.h>
#include <drivers/platform_clock.h>

//
// Stores a reference to each active UART object.
// Used to grab the relevant UART object for its interrupt.
//
uart_t *active_uart_objects[4];


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
	// USART0
	{
		.tx = { .group = 9, .pin = 5, .function = 7 },
		.rx = { .group = 9, .pin = 6, .function = 7 }

		// TODO: add flow control pins!
	},

	// FIXME: add the other UARTS!
	// UART1
	{
		.tx = { .group = 1, .pin = 13, .function = 1 },
		.rx = { .group = 1, .pin = 14, .function = 1 }

		// TODO: add flow control pins!
	},
	// USART2
	{
		.tx = { .group = 1, .pin = 15, .function = 1 },
		.rx = { .group = 1, .pin = 16, .function = 1 }

		// TODO: add flow control pins!
	},
	// USART3
	{
		.tx = { .group = 2, .pin = 3, .function = 2 },
		.rx = { .group = 2, .pin = 4, .function = 2 }

		// TODO: add flow control pins!
	},
};


// Imports from the local UART driver. These aren't part of the public API,
// so they're not defined in uart.h.
void uart_interrupt(uart_t *uart);


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
 * Low-level UART interrupt handlers.
 * These grab the active UART object for the relevant interrupt, and call the main ISR.
 */

static void platform_uart0_interrupt(void)
{
	uart_interrupt(active_uart_objects[0]);
}

static void platform_uart1_interrupt(void)
{
	uart_interrupt(active_uart_objects[1]);
}

static void platform_uart2_interrupt(void)
{
	uart_interrupt(active_uart_objects[2]);
}

static void platform_uart3_interrupt(void)
{
	uart_interrupt(active_uart_objects[3]);
}




/**
 * Performs platform-specific initialization for the system's UART interrupt.
 */
int platform_uart_set_up_interrupt(uart_t *uart)
{
	uint32_t irq_number;

	// Look-up table of per-UART interrupt handlers.
	const vector_table_entry_t irq_handlers[] = {
		platform_uart0_interrupt, platform_uart1_interrupt,
		platform_uart2_interrupt, platform_uart3_interrupt
	};

	const uint32_t irq_numbers[] = {
		USART0_IRQ, UART1_IRQ, USART2_IRQ, USART3_IRQ
	};

	// Store the current UART object, so we can find it during our interrupt handler.
	active_uart_objects[uart->number] = uart;

	// Enable the relevant interrupt in the NVIC.
	irq_number = irq_numbers[uart->number];
	vector_table.irqs[irq_number] = irq_handlers[uart->number];
	platform_enable_interrupt(irq_number);

	return 0;
}


/**
 * @return the frequency of the clock driving this UART, in Hz
 */
uint32_t platform_uart_get_parent_clock_frequency(uart_t *uart)
{
	return platform_get_branch_clock_frequency(uart->platform_data.clock);
}
