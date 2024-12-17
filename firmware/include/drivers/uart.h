
/*
 * This file is part of libgreat
 *
 * Generic UART driver header.
 */


#include <toolchain.h>
#include <drivers/platform_uart.h>
#include <drivers/memory/ringbuffer.h>


#ifndef __LIBGREAT_UART_H__
#define __LIBGREAT_UART_H__

typedef enum {
	ONE_STOP_BIT  = 0,
	TWO_STOP_BITS = 1
} uart_stop_bits_t;


typedef enum {
	NO_PARITY            = 0b000,
	ODD_PARITY           = 0b001,
	EVEN_PARITY          = 0b011,
	PARITY_STUCK_AT_ONE  = 0b101,
	PARITY_STUCK_AT_ZERO = 0b111,
} uart_parity_type_t;

typedef enum {
	FIVE_DATA_BITS  = 0,
	SIX_DATA_BITS   = 1,
	SEVEN_DATA_BITS = 2,
	EIGHT_DATA_BITS = 3,
} data_bit_length_t;


/**
 * Object representing a UART device.
 */
typedef struct uart {

	/**
	 * User configuration fields.
	 * These should be set before a call to uart_init.
	 */
	uart_number_t             number;
	uint32_t                  baud_rate;
	uint8_t                   data_bits;
	uart_stop_bits_t          stop_bits;
	uart_parity_type_t        parity_mode;

	// The size of the buffer to be allocated for buffered reads/writes.
	// If this is set to 0, only synchronous reads and writes are supported.
	size_t buffer_size;


	/**
	 * Private fields -- for driver use only. :)
	 */

	// The actual baud rate, accounting for errors due to limited-precision dividers.
	uint32_t baud_rate_achieved;

	// UART registers.
	platform_uart_registers_t *reg;

	// Platform-specific data.
	platform_uart_t platform_data;

	// Pointer to a ringbuffer used for asynchronous reads and writes.
	// May be null if only synchronous reads and writes are supported.
	ringbuffer_t rx_buffer;
	ringbuffer_t tx_buffer;

} uart_t;

/**
 * UART implementation functions.
 */


/**
 * Sets up a platform UART for use.
 *
 * @param uart A UART structure with configuration fields pre-populated. See above.
 */
int uart_init(uart_t *uart);


/**
 * Platform-specific functions.
 */

/**
 * Performs platform-specific initialization for the given UART.
 */
int platform_uart_init(uart_t *uart);

/**
 * @return the frequency of the clock driving this UART, in Hz
 */
uint32_t platform_uart_get_parent_clock_frequency(uart_t *uart);


/**
 * Performs platform-specific initialization for the system's UART interrupt.
 */
int platform_uart_set_up_interrupt(uart_t *uart);


/**
 * Perform a UART transmit, but block until the transmission is accepted.
 */
void uart_transmit_synchronous(uart_t *uart, uint8_t byte);



/**
 * Reads all available data from the asynchronous receive buffer --
 * essentially any data received since the last call to a read function.
 *
 * @param buffer The buffer to read into.
 * @param count The maximum amount of data to read. Often, this is the size of
 *              your buffer.
 * @return The total number of bytes read.
 */
size_t uart_read(uart_t *uart, void *buffer, size_t count);

#endif
