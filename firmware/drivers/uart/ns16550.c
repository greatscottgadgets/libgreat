/*
 * This file is part of libgreat
 *
 * NS16550-compatible UART drivers.
 */


#include <math.h>
#include <errno.h>
#include <debug.h>
#include <stdlib.h>
#include <toolchain.h>

#include <drivers/uart.h>
#include <drivers/arm_system_control.h>

typedef enum {
	IRQ_RECEIVE_DATA_AVAILABLE = 0x2,
} pending_interrupt_t;


// FIXME HACK: statically allocate UART buffers
static uint8_t uart_rx_buffer[256];
static uint8_t uart_tx_buffer[256];

static uint32_t divide_and_round(uint32_t numerator, uint32_t denominator)
{
	// Compute twice the desired numerator. This effectively allows us to use the LSB
	// as a single fixed-point place, worth 0.5.
	uint32_t twice_numerator = numerator * 2;
	uint32_t twice_dividend  = twice_numerator / denominator;

	// Return a rounded version of the relevant division. This works because we're in fixed-point;
	// we effectively add 0.5, and the truncate to an integer by removing the digit past our binary point. :)
	return (twice_dividend + 1) / 2;
}


/**
 * Computes the actual, achieved baud rate given our computed baud parameters.
 */
static uint32_t get_actual_baud_rate(uint32_t clk_freq, uint8_t multiplier,
	uint8_t fractional_divisor, uint16_t integer_divisor)
{
	// If our fractional component is zero, the fractional divisor isn't being used.
	// Compute an integer divisor directly.
	if (fractional_divisor == 0) {
		return clk_freq / integer_divisor;
	} else {
		float fractional_ratio = 1 + ((float)fractional_divisor / (float)multiplier);
		float actual_divisor = 16 * integer_divisor * fractional_ratio;
		return (uint32_t)(clk_freq / actual_divisor);
	}
}

/**
 * @return a number roughly proportional to the amount we deviate from the desired baud rate;
 *     where a higher number indicates a worse error
 */
static uint32_t baud_rate_error(int32_t baud_desired, uint32_t clk_freq, uint8_t multiplier,
	uint8_t fractional_divisor, uint16_t divisor)
{
	int32_t baud_achieved = get_actual_baud_rate(clk_freq, multiplier, fractional_divisor, divisor);

	// Always scale things so a larger number indicates a larger error.
	if (baud_achieved < baud_desired) {
		return (uint32_t)(baud_desired - baud_achieved);
	} else {
		return (uint32_t)(baud_achieved - baud_desired);
	}
}

/**
 * Compute the optimal integer divider, assuming a given
 */
static uint16_t find_integer_divisor_for_fractional_divisor(uint32_t parent_clock,
	uint32_t desired_baud, uint8_t mul, uint8_t div)
{
	// If our fractional component is zero, the fractional divisor isn't being used.
	// Compute an integer divisor directly.
	if (div == 0) {
		return divide_and_round(parent_clock, desired_baud);

	} else {
		return divide_and_round((parent_clock * mul), (16 * desired_baud * (mul + div)));
	}
}


/**
 * Applies the provided baud rate to the given UART.
 * @return the actual, achieved baud rate, or 0 if the baud rate was unachievable
 */
uint32_t uart_set_baud_rate(uart_t *uart, uint32_t baud_rate)
{
	uint8_t optimal_mul            = 0;
	uint8_t optimal_fractional_div = 0;
	uint16_t optimal_integer_div   = 0;
	uint32_t current_error = -1;

	// Retrieve the clock frequency of our parent clock.
	uint32_t parent_clock_freq = platform_uart_get_parent_clock_frequency(uart);

	// Store the updated baud rate.
	uart->baud_rate = baud_rate;

	// General algorithm to figure out the optimal baud rate:
	// no one else seems to use a closed-form solution; and there are only 255
	// possible values to search through. Brute force the damned thing.
	for (uint8_t div = 0; div < 14; ++div) {
		for (uint8_t mul = 1; mul < 16; ++mul) {
			uint32_t computed_error;
			uint32_t integer_divisor;

			// Sanity check: our divider can't be more than our multiplier, or we'd
			// wind up with a divisor that rounds to zero. That'd produce a UART clock
			// that never ticks.
			if (div >= mul) {
				continue;
			}

			// All values with div == 0 are the same, as this means the integer divider
			// is disabled. Skip checking all the multiplier values.
			if ((div == 0) && (mul > 1)) {
				continue;
			}

			// Compute the integer divisor we'd use with the given divisor.
			integer_divisor = find_integer_divisor_for_fractional_divisor(parent_clock_freq, baud_rate, mul, div);

			// If the integer divisor won't fit in our integer divisor register,
			// this can't be right. Skip it.
			if (integer_divisor >= (1 << 16)) {
				continue;
			}

			// If we've arrived at an integer divisor of 0, our div/mul are invalid.
			// Skip these values.
			if (integer_divisor == 0) {
				continue;
			}

			// Finally, we have valid div and mul values!
			computed_error = baud_rate_error(baud_rate, parent_clock_freq, mul, div, integer_divisor);

			// If this set of divisor values produce better errors, make them our new candidate
			// for most optimal.
			if (computed_error < current_error) {
				optimal_fractional_div = div;
				optimal_integer_div    = integer_divisor;
				optimal_mul            = mul;
				current_error          = computed_error;
			}
		}
	}

	// If we didn't find any sensical values, error out.
	if (optimal_mul == 0) {
		return 0;
	}

	// If we have sensical values, apply them.
	uart->reg->fractional_divisor    = optimal_fractional_div;
	uart->reg->fractional_multiplier = optimal_mul;

	// The integer divisor is gated by the "DLAB", the divisor latch access enabled bit.
	// We'll set that before applying our value, and clear it after.
	uart->reg->divior_latch_access_enabled = 1;
	uart->reg->divisor_lsb = optimal_integer_div & 0xFF;
	uart->reg->divisor_msb = optimal_integer_div >> 8;
	uart->reg->divior_latch_access_enabled = 0;

	// Compute the actual baud rate achieved; and return it.
	uart->baud_rate_achieved = get_actual_baud_rate(parent_clock_freq, optimal_mul,
		optimal_fractional_div, optimal_integer_div);


	pr_debug("uart: achieved %" PRIu32 " for a desired baud of %" PRIu32 ".\n", uart->baud_rate_achieved,
		uart->baud_rate);
	pr_debug("uart: (divider values were: idiv=%d fdiv=%d mul=%d)\n",
		optimal_integer_div, optimal_fractional_div, optimal_mul);

	return uart->baud_rate_achieved;
}


/**
 * Sets up a platform UART for use.
 *
 * @param uart A UART structure with configuration fields pre-populated. See uart.h.
 */
int uart_init(uart_t *uart)
{
	// Perform the platform-specific initialization for the given UART.
	int rc = platform_uart_init(uart);
	if (rc) {
		return rc;
	}

	// Initialize our platform's FIFOs, and clear their contents.
	uart->reg->fifo_enabled              = 1;
	uart->reg->rx_fifo_reset_in_progress = 1;
	uart->reg->rx_fifo_reset_in_progress = 1;

	// For now, start with FIFOs disabled.
	// TODO: don't do this; use the fifos properly
	uart->reg->fifo_enabled = 0;

	// Clear out any data remaining in our rx register.
	while (uart->reg->rx_data_ready) {
		(void)uart->reg->receive_buffer;
	}

	// TODO: possibly drain the UART holding register?

	// Disable all interrupts.
	uart->reg->interrupt_enable_register = 0;

	// Set up our packet framing.
	uart->reg->word_length       = uart->data_bits - 5;
	uart->reg->parity_mode       = uart->parity_mode;
	uart->reg->use_two_stop_bits = (uart->stop_bits == TWO_STOP_BITS);

	// For now, don't support line-breaking.
	uart->reg->use_break         = 0;

	// Finally, set the baud rate, and enable transmission.
	uart_set_baud_rate(uart, uart->baud_rate);
	if (uart->baud_rate_achieved == 0) {
		return EINVAL;
	}

	// Enable transmission.
	uart->reg->enable_transmit = true;

	// If we're going to allocate a buffer, we can perform asynchronous reads and writes.
	// Accordingly, we'll set up interrupts to fill / drain the rx and tx buffers.
	if (uart->buffer_size) {

		// Allocate memory for our Rx buffer..
		// FIXME: malloc this!
		ringbuffer_init(&uart->rx_buffer, uart_rx_buffer, uart->buffer_size);

		// ... and for our tx buffer.
		// FIXME: malloc this!
		ringbuffer_init(&uart->tx_buffer, uart_tx_buffer, uart->buffer_size);

		// If we couldn't allocate either buffer, disable synchronous operations and return a warning code.
		if (!uart->rx_buffer.buffer || !uart->tx_buffer.buffer) {
			pr_warning("uart: warning: could not allocate memory for our async operations buffer!\n");
			pr_warning("uart: asynchronous operations disabled -- all reads/writes will be synchronous!");
			uart->buffer_size = 0;

			free(uart->rx_buffer.buffer);
			free(uart->tx_buffer.buffer);

			return ENOMEM;
		}

		// Set up an interrupt to handle UART interrupt events.
		rc = platform_uart_set_up_interrupt(uart);
		if (rc) {
			return rc;
		}

		// Enable an interrupt when we receive data; which will be used to populate our ring buffer.
		uart->reg->receive_data_available_interrupt_enabled = true;
	}

	return 0;
}


void uart_data_ready_interrupt(uart_t *uart)
{
	uint8_t rx_data = uart->reg->receive_buffer;
	ringbuffer_enqueue_overwrite(&uart->rx_buffer, rx_data);
}



/**
 * Function called as the main handler for a UART interrupt.
 */
void uart_interrupt(uart_t *uart)
{
	// If there are no UART interrupts pending, there's nothing to do.
	// Return early.
	if (uart->reg->no_interrupts_pending) {
		return;
	}

	// If this is a "new data received" event, handle it.
	if(uart->reg->pending_interrupt == IRQ_RECEIVE_DATA_AVAILABLE) {
		uart_data_ready_interrupt(uart);
	}
}



/**
 * Reads all available data from the asynchronous receive buffer --
 * essentially any data received since the last call to a read function.
 *
 * @param buffer The buffer to read into.
 * @param count The maximum amount of data to read. Often, this is the size of
 *              your buffer.
 * @return The total number of bytes read.
 */
size_t uart_read(uart_t *uart, void *buffer, size_t count)
{
	uint8_t *byte_buffer = buffer;
	size_t data_read = 0;

	// Special case: if we're in synchronous mode, read and return
	// a single byte -- this handles the case where this is called after
	// an allocation fail-out.
	if (uart->buffer_size == 0) {
		// TODO: implement
		return 0;
	}

	// Read count bytes from the buffer, or as much as is available.
	for (unsigned i = 0; i < count; ++i) {
		if (ringbuffer_empty(&uart->rx_buffer)) {
			break;
		}

		byte_buffer[i] = ringbuffer_dequeue(&uart->rx_buffer);
		data_read++;
	}

	// Return the actual amount read.
	return data_read;
}



/**
 * Perform a UART transmit, but block until the transmission is accepted.
 */
void uart_transmit_synchronous(uart_t *uart, uint8_t byte)
{
	while(!uart->reg->transmit_holding_register_empty);
	uart->reg->transmit_buffer = byte;
}
