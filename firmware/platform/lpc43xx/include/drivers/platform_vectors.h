/*
 * LPC43xx::M4 exception and interrupt handling code.
 *
 * This file is part of libgreat
 */


#include <toolchain.h>
#include <stdint.h>

#ifndef __LIBGREAT_DRIVER_LPC43XX_VECTORS__
#define __LIBGREAT_DRIVER_LPC43XX_VECTORS__

/**
 * Type for ISRs.
 */
typedef void (*interrupt_service_routine_t)(void);

/**
 * IRQ numbers for each of the LPC43xx interrupts.
 */
typedef enum {
	DAC_IRQ         = 0,
	M0CORE_IRQ      = 1,
	DMA_IRQ         = 2,
	ETHERNET_IRQ    = 5,
	SDIO_IRQ        = 6,
	LCD_IRQ         = 7,
	USB0_IRQ        = 8,
	USB1_IRQ        = 9,
	SCT_IRQ         = 10,
	RITIMER_IRQ     = 11,
	TIMER0_IRQ      = 12,
	TIMER1_IRQ      = 13,
	TIMER2_IRQ      = 14,
	TIMER3_IRQ      = 15,
	MCPWM_IRQ       = 16,
	ADC0_IRQ        = 17,
	I2C0_IRQ        = 18,
	I2C1_IRQ        = 19,
	SPI_IRQ         = 20,
	ADC1_IRQ        = 21,
	SSP0_IRQ        = 22,
	SSP1_IRQ        = 23,
	USART0_IRQ      = 24,
	UART1_IRQ       = 25,
	USART2_IRQ      = 26,
	USART3_IRQ      = 27,
	I2S0_IRQ        = 28,
	I2S1_IRQ        = 29,
	SPIFI_IRQ       = 30,
	SGPIO_IRQ       = 31,
	PIN_INT0_IRQ    = 32,
	PIN_INT1_IRQ    = 33,
	PIN_INT2_IRQ    = 34,
	PIN_INT3_IRQ    = 35,
	PIN_INT4_IRQ    = 36,
	PIN_INT5_IRQ    = 37,
	PIN_INT6_IRQ    = 38,
	PIN_INT7_IRQ    = 39,
	GINT0_IRQ       = 40,
	GINT1_IRQ       = 41,
	EVENTROUTER_IRQ = 42,
	C_CAN1_IRQ      = 43,
	ATIMER_IRQ      = 46,
	RTC_IRQ         = 47,
	WWDT_IRQ        = 49,
	C_CAN0_IRQ      = 51,
	QEI_IRQ         = 52,

	// Total number of interrupts.
	PLATFORM_TOTAL_IRQS    = 53,
} platform_irq_number_t;


/**
 * For now, use the IRQ number directly as an opaque IRQ type.
 */
typedef platform_irq_number_t platform_irq_t;

/**
 * Definition that abstracts away the size/shape of an ISR table.
 */
typedef interrupt_service_routine_t platform_irq_table_t[PLATFORM_TOTAL_IRQS];

/**
 * Type that helps to abstract how a given platform encodes interrupt priority.
 */
typedef uint8_t platform_interrupt_priority_t;

#endif
