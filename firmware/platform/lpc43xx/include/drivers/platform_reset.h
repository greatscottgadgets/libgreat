/**
 * This file is part of libgreat
 *
 * LPC43xx reset generation/control driver
 */


#ifndef __LIBGREAT_PLATFORM_RESET_H__
#define __LIBGREAT_PLATFORM_RESET_H__

#include <toolchain.h>
#include <drivers/platform_config.h>

/**
 * Structure representing the reset control registers.
 */
typedef volatile struct ATTR_PACKED {

	// Reset control register 0.
	struct {
		uint32_t core_reset       : 1;
		uint32_t perpiheral_reset : 1;
		uint32_t master_reset     : 1;
		uint32_t                  : 1;
		uint32_t watchdog_reset   : 1;
		uint32_t creg_reset       : 1;
		uint32_t                  : 1;
		uint32_t                  : 1;
		uint32_t bus_reset        : 1;
		uint32_t scu_reset        : 1;
		uint32_t                  : 2;
		uint32_t m0_core_reset    : 1;
		uint32_t m4_core_reset    : 1;
		uint32_t                  : 2;
		uint32_t lcd_reset        : 1;
		uint32_t usb0_reset       : 1;
		uint32_t usb1_reset       : 1;
		uint32_t dma_reset        : 1;
		uint32_t sdio_reset       : 1;
		uint32_t emc_reset        : 1;
		uint32_t ethernet_reset   : 1;
		uint32_t                  : 2;
		uint32_t flash_a_reset    : 1;
		uint32_t                  : 1;
		uint32_t eeprom_reset     : 1;
		uint32_t gpio_reset       : 1;
		uint32_t flash_b_reset    : 1;
		uint32_t                  : 2;
	};

	// Reset control register 1.
	struct {
		uint32_t timer0_reset     : 1;
		uint32_t timer1_reset     : 1;
		uint32_t timer2_reset     : 1;
		uint32_t timer3_reset     : 1;
		uint32_t rtimer_reset     : 1;
		uint32_t sct_reset        : 1;
		uint32_t motoconpwm_reset : 1;
		uint32_t qei_reset        : 1;
		uint32_t adc0_reset       : 1;
		uint32_t adc1_reset       : 1;
		uint32_t dac_reset        : 1;
		uint32_t                  : 1;
		uint32_t uart0_reset      : 1;
		uint32_t uart1_reset      : 1;
		uint32_t uart2_reset      : 1;
		uint32_t uart3_reset      : 1;
		uint32_t i2c0_reset       : 1;
		uint32_t i2c1_reset       : 1;
		uint32_t ssp0_reset       : 1;
		uint32_t ssp1_reset       : 1;
		uint32_t i2s_reset        : 1;
		uint32_t spifi_reset      : 1;
		uint32_t can1_reset       : 1;
		uint32_t can0_reset       : 1;
		uint32_t m0app_reset      : 1;
		uint32_t sgpio_reset      : 1;
		uint32_t spi_reset        : 1;
		uint32_t                  : 1;
		uint32_t adchs_reset      : 1;
		uint32_t                  : 3;
	};

	RESERVED_WORDS(2);


	// TODO: fill in the bits for these registers!
	uint32_t reset_status[4];
	uint32_t reset_active_status[2];
	uint32_t reset_ext_stat[64];

} platform_reset_register_block_t;


ASSERT_OFFSET(platform_reset_register_block_t, reset_status, 0x10);

/**
 * Watchdog controller drivers.
 */
typedef volatile struct ATTR_PACKED {

	struct {
		uint8_t enable                : 1;
		uint8_t reset_enable          : 1;
		uint8_t timed_out             : 1;
		uint8_t interrupt_on_timeout  : 1;
		uint8_t limit_update_interval : 1;
		uint8_t                       : 3;
	};

	RESERVED_BYTES(3);

	struct {
		uint32_t timeout               : 24;
		uint32_t                       :  8;
	};

	// Watchdog feed register.
	uint8_t feed;
	RESERVED_BYTES(3);

	struct {
		uint32_t timer_value           : 24;
		uint32_t                       :  8;
	};


	RESERVED_WORDS(1);

	struct {
		uint32_t warning_threshold     : 10;
		uint32_t                       : 22;
	};
	struct {
		uint32_t valid_feed_threshold  : 24;
		uint32_t                       :  8;
	};

} platform_watchdog_register_block_t;


/**
 * Return a reference to the LPC43xx's RGU block.
 */
platform_reset_register_block_t *get_platform_reset_registers(void);


/**
 * Software reset the entire system.
 *
 * @param true iff the always-on power domain should be included
 */
void platform_software_reset(bool include_always_on_domain);


/**
 * @return true iff the system reset was an unintentional watchdog reset
 * 		tries to ignore cases where a soft-reset used the watchdog to implement the reset itself
 */
bool platform_reset_was_watchdog_timeout(void);

/**
 * Clears any system state necessary to track the system's state across resets.
 */
void platform_initialize_reset_driver(void);

#endif
