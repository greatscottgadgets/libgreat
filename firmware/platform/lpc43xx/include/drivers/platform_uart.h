/*
 * This file is part of libgreat
 *
 * LPC43xx timer drivers
 */

#ifndef __LIBGREAT_PLATFORM_UART_H__
#define __LIBGREAT_PLATFORM_UART_H__

#include <toolchain.h>
#include <drivers/platform_clock.h>

typedef struct uart uart_t;

/**
 * UART numbers for each supported UART on the LPC43xx.
 */
typedef enum {
	UART0 = 0,
	UART1 = 1,
	UART2 = 2,
	UART3 = 3,
} uart_number_t;

/**
 * General UART constants.
 */
enum {
	NUM_UARTS = 4
};


/**
 * Register layout for LPC43xx UARTs.
 */
typedef volatile struct ATTR_PACKED {

	union {
		struct {
			uint32_t receive_buffer :  8;
			uint32_t                : 24;
		};
		uint32_t transmit_buffer;
		uint32_t divisor_lsb;
	};

	union {
		uint32_t divisor_msb;

		// Interrupt enable register.
		struct {
			uint32_t receive_data_available_interrupt_enabled          :  1;
			uint32_t transmit_holding_register_empty_interrupt_enabled :  1;
			uint32_t receive_line_interrupt_enabled                    :  1;
			uint32_t                                                   :  5;
			uint32_t end_of_autobaud_interrupt_enabled                 :  1;
			uint32_t autobaud_timeout_interrupt_enabled                :  1;
			uint32_t                                                   : 22;
		};
		uint32_t interrupt_enable_register;
	};

	union {

		// Interrupt identification register.
		struct {
			uint32_t no_interrupts_pending              :  1;
			uint32_t pending_interrupt                  :  3;
			uint32_t                                    :  2;
			uint32_t autobaud_success_interrupt_pending :  1;
			uint32_t autobaud_timeout_interrupt_pending :  1;
			uint32_t                                    : 24;
		};

		// FIFO control register.
		struct {
			uint32_t fifo_enabled              :  1;
			uint32_t rx_fifo_reset_in_progress :  1;
			uint32_t tx_fifo_reset_in_progress :  1;
			uint32_t use_dma                   :  1;
			uint32_t                           :  2;
			uint32_t receive_trigger_level     :  2;
			uint32_t                           : 24;
		};

		uint32_t interrupt_identification_register;
	};

	// Line control register.
	union {
		struct {
			uint32_t word_length                 : 2;
			uint32_t use_two_stop_bits           : 1;
			uint32_t parity_mode                 : 3;
			uint32_t use_break                   : 1;
			uint32_t divior_latch_access_enabled : 1;
		};
		uint32_t line_control_register;
	};

	RESERVED_WORDS(1);

	// Line status register.
	struct {
		uint32_t rx_data_ready                        : 1;
		uint32_t overrun_error_occurred               : 1;
		uint32_t parity_error_occurred                : 1;
		uint32_t framing_error_occurred               : 1;
		uint32_t break_occurred                       : 1;
		uint32_t transmit_holding_register_empty      : 1;
		uint32_t transmitter_empty                    : 1;
		uint32_t tx_fifo_error_occurred               : 1;
		uint32_t transmitted_character_error_occurred : 1;
		uint32_t                                      : 23;
	};

	RESERVED_WORDS(1);

	uint32_t scratch_pad_register;
	uint32_t autobaud_control;
	uint32_t irda_control;

	// Fractional divider register.
	union {
		struct {
			uint32_t fractional_divisor    :  4;
			uint32_t fractional_multiplier :  4;
			uint32_t                       : 24;
		};

		uint32_t fractional_divisor_control;
	};


	uint32_t oversampling_control;

	RESERVED_WORDS(4);

	uint32_t half_duplex_enable_register;

	RESERVED_WORDS(1);

	uint32_t smart_card_interface_control_register;
	uint32_t rs485_control_register;
	uint32_t rs485_address_match_register;
	uint32_t rs485_direction_control_delay;
	uint32_t synchronous_mode_control;

	struct {
		uint32_t enable_transmit :  1;
		uint32_t                 : 31;
	};

} platform_uart_registers_t;

ASSERT_OFFSET(platform_uart_registers_t, interrupt_identification_register, 0x8);
ASSERT_OFFSET(platform_uart_registers_t, line_control_register, 0xC);
ASSERT_OFFSET(platform_uart_registers_t, fractional_divisor_control, 0x28);


/**
 * Platform-specific, per-UART-instance data.
 */
typedef struct {

	// The clock that controls the relevant UART.
	platform_branch_clock_t *clock;

} platform_uart_t;

#endif
