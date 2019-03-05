/*
 * This file is part of libgreat
 *
 * LPC43xx clock generation/control driver
 */


#ifndef __LIBGREAT_PLATFORM_CLOCK_H__
#define __LIBGREAT_PLATFORM_CLOCK_H__

#include <toolchain.h>
#include <drivers/platform_config.h>

/**
 * Register field that describes a base clock in the Clock Generation Unit.
 */
typedef volatile union {
	struct ATTR_PACKED{
		uint32_t power_down            :  1;
		uint32_t                       :  1;
		uint32_t divisor               :  8;
		uint32_t                       :  1;
		uint32_t block_during_changes  :  1;
		uint32_t                       : 12;
		uint32_t source                :  5;
		uint32_t                       :  3;
	};
	uint32_t all_bits;
} platform_base_clock_register_t;

/**
 * For now, assume that base clock objects are equivalent to references
 * to their registers.
 */
typedef platform_base_clock_register_t platform_base_clock_t;


/**
 * Various different sources that can drive various clock units.
 */
typedef enum {

	// Slow oscillators, both external (RTC) and internal (IRC).
	CLOCK_SOURCE_32KHZ_OSCILLATOR     = 0x00,
	CLOCK_SOURCE_INTERNAL_OSCILLATOR  = 0x01,

	// Clock inputs -- these accept clocks directly on a GPIO pin.
	CLOCK_SOURCE_ENET_RX_CLOCK        = 0x02,
	CLOCK_SOURCE_ENET_TX_CLOCK        = 0x03,
	CLOCK_SOURCE_GP_CLOCK_INPUT       = 0x04,

	// Main clock oscillator.
	CLOCK_SOURCE_XTAL_OSCILLATOR      = 0x06,

	// Derived clocks -- including PLLs and dividiers.
	CLOCK_SOURCE_PLL0_USB             = 0x07,
	CLOCK_SOURCE_PLL0_AUDIO           = 0x08,
	CLOCK_SOURCE_PLL1                 = 0x09,
	CLOCK_SOURCE_DIVIDER_A_OUT        = 0x0c,
	CLOCK_SOURCE_DIVIDER_B_OUT        = 0x0d,
	CLOCK_SOURCE_DIVIDER_C_OUT        = 0x0e,
	CLOCK_SOURCE_DIVIDER_D_OUT        = 0x0f,
	CLOCK_SOURCE_DIVIDER_E_OUT        = 0x10,

	// Total number of actual clock sources.
	CLOCK_SOURCE_COUNT                = 0x11,

	// Special value -- used to represent an unused or invalid clock.
	CLOCK_SOURCE_NONE                 = 0x1D,

	// Special value -- the primary clock input to the system, which is usually the external oscillator.
	// This is the clock that's used to drive e.g. the internal PLL that drives the main system clock.
	// The downstream software can override this by defining platform_determine_primary_clock_input().
	CLOCK_SOURCE_PRIMARY_INPUT        = 0x1E,

	// Special value -- use the system's "primary" clock source, which is usually an internal PLL.
	// The downstream software can override this by defining platform_determine_primary_clock_source().
	CLOCK_SOURCE_PRIMARY              = 0x1F,

} clock_source_t;


/**
 * Register pair that provides control and status for each downstream clock.
 */
typedef volatile struct ATTR_PACKED {

	/**
	 * Values to be applied to the given clock; these may not
	 * take effect immediately. Check the current register for the
	 * relevant value.
	 */
	struct {
		uint32_t enable                                 :  1;
		uint32_t disable_when_bus_transactions_complete :  1;
		uint32_t wake_after_powerdown                   :  1;
		uint32_t                                        :  2;
		uint32_t divisor                                :  3;
		uint32_t                                        : 19;
		uint32_t current_divisor                        :  3;
		uint32_t                                        :  2;
	} control;

	/**
	 * Values currently being used by hardware for each clock.
	 */
	struct {
		uint32_t enabled                                :  1;
		uint32_t disable_when_bus_transactions_complete :  1;
		uint32_t wake_after_powerdown                   :  1;
		uint32_t                                        :  2;
		uint32_t disabled                               :  1;
		uint32_t                                        : 26;
	} current;

} platform_branch_clock_register_t;

ASSERT_OFFSET(platform_branch_clock_register_t, current, 0x4);


/**
 * For now, consider branch clocks and their registers the same thing.
 */
typedef platform_branch_clock_register_t platform_branch_clock_t;


/**
 * Structure representing the clock control registers.
 */
typedef volatile struct ATTR_PACKED {

	// Power management register.
	struct {
		// Disable wakeable clocks.
		// Powers down all clocks that can be automatically resumed after a power-down.
		uint32_t power_down      :  1;
		uint32_t                 : 31;
	} ccu1;

	// Base clock status for CCU1.
	struct {
		uint32_t apb3_needed    :  1;
		uint32_t apb1_needed    :  1;
		uint32_t spifi_needed   :  1;
		uint32_t m4_needed      :  1;
		uint32_t                :  2;
		uint32_t periph_needed  :  1;
		uint32_t usb0_needed    :  1;
		uint32_t usb1_needed    :  1;
		uint32_t spi_needed     :  1;
		uint32_t                : 22;
	};

	RESERVED_WORDS(62);

	// APB3 Clock register pairs.
	struct {
		platform_branch_clock_register_t bus;
		platform_branch_clock_register_t i2c1;
		platform_branch_clock_register_t dac;
		platform_branch_clock_register_t adc0;
		platform_branch_clock_register_t adc1;
		platform_branch_clock_register_t can0;
	} apb3;

	RESERVED_WORDS(52);

	// APB1 Clock register pairs.
	struct {
		platform_branch_clock_register_t bus;
		platform_branch_clock_register_t motocon_pwm;
		platform_branch_clock_register_t i2c0;
		platform_branch_clock_register_t i2s;
		platform_branch_clock_register_t can1;
	} apb1;

	RESERVED_WORDS(54);

	platform_branch_clock_register_t spifi;

	RESERVED_WORDS(62);

	// M4 core related clcoks.
	struct {
		platform_branch_clock_register_t bus;
		platform_branch_clock_register_t spifi;
		platform_branch_clock_register_t gpio;
		platform_branch_clock_register_t lcd;
		platform_branch_clock_register_t ethernet;
		platform_branch_clock_register_t usb0;
		platform_branch_clock_register_t emc;
		platform_branch_clock_register_t sdio;
		platform_branch_clock_register_t dma;
		platform_branch_clock_register_t core;
		RESERVED_WORDS(6);
		platform_branch_clock_register_t sct;
		platform_branch_clock_register_t usb1;
		platform_branch_clock_register_t emcdiv;
		platform_branch_clock_register_t flasha;
		platform_branch_clock_register_t flashb;
		platform_branch_clock_register_t m0app;
		platform_branch_clock_register_t adchs;
		platform_branch_clock_register_t eeprom;
		RESERVED_WORDS(22);
		platform_branch_clock_register_t wwdt;
		platform_branch_clock_register_t usart0;
		platform_branch_clock_register_t uart1;
		platform_branch_clock_register_t ssp0;
		platform_branch_clock_register_t timer0;
		platform_branch_clock_register_t timer1;
		platform_branch_clock_register_t scu;
		platform_branch_clock_register_t creg;
		RESERVED_WORDS(48);
		platform_branch_clock_register_t ritimer;
		platform_branch_clock_register_t usart2;
		platform_branch_clock_register_t usart3;
		platform_branch_clock_register_t timer2;
		platform_branch_clock_register_t timer3;
		platform_branch_clock_register_t ssp1;
		platform_branch_clock_register_t qei;
	} m4;

	RESERVED_WORDS(50);

	// Peripheral bus.
	struct {
		platform_branch_clock_register_t bus;
		platform_branch_clock_register_t core;
		platform_branch_clock_register_t sgpio;
	} periph;

	RESERVED_WORDS(58);

	platform_branch_clock_register_t usb0;

	RESERVED_WORDS(62);

	platform_branch_clock_register_t usb1;

	RESERVED_WORDS(62);

	platform_branch_clock_register_t spi;

	RESERVED_WORDS(62);

	platform_branch_clock_register_t adchs;

	// Space until CGU2
	RESERVED_WORDS(318);

	// Power management register.
	struct {
		// Disable wakeable clocks.
		// Powers down all clocks that can be automatically resumed after a power-down.
		uint32_t power_down      :  1;
		uint32_t                 : 31;
	} ccu2;

	// Base clock status.
	struct {
		uint32_t                :  1;
		uint32_t uart3_needed  :  1;
		uint32_t uart2_needed  :  1;
		uint32_t uart1_needed  :  1;
		uint32_t uart0_needed  :  1;
		uint32_t ssp1_needed   :  1;
		uint32_t ssp0_needed   :  1;
		uint32_t                : 25;
	};
	RESERVED_WORDS(62);

	platform_branch_clock_register_t audio;
	RESERVED_WORDS(62);

	platform_branch_clock_register_t usart3;
	RESERVED_WORDS(62);

	platform_branch_clock_register_t usart2;
	RESERVED_WORDS(62);

	platform_branch_clock_register_t uart1;
	RESERVED_WORDS(62);

	platform_branch_clock_register_t usart0;
	RESERVED_WORDS(62);

	platform_branch_clock_register_t ssp1;
	RESERVED_WORDS(62);

	platform_branch_clock_register_t ssp0;
	RESERVED_WORDS(62);

	platform_branch_clock_register_t sdio;
	RESERVED_WORDS(62);


} platform_clock_control_register_block_t;


ASSERT_OFFSET(platform_clock_control_register_block_t, apb3,       0x0100);
ASSERT_OFFSET(platform_clock_control_register_block_t, apb1,       0x0200);
ASSERT_OFFSET(platform_clock_control_register_block_t, spifi,      0x0300);
ASSERT_OFFSET(platform_clock_control_register_block_t, m4,         0x0400);
ASSERT_OFFSET(platform_clock_control_register_block_t, m4.core,    0x0448);
ASSERT_OFFSET(platform_clock_control_register_block_t, m4.sct,     0x0468);
ASSERT_OFFSET(platform_clock_control_register_block_t, m4.wwdt,    0x0500);
ASSERT_OFFSET(platform_clock_control_register_block_t, m4.ritimer, 0x0600);
ASSERT_OFFSET(platform_clock_control_register_block_t, periph,     0x0700);
ASSERT_OFFSET(platform_clock_control_register_block_t, usb0,       0x0800);
ASSERT_OFFSET(platform_clock_control_register_block_t, usb1,       0x0900);
ASSERT_OFFSET(platform_clock_control_register_block_t, spi,        0x0A00);
ASSERT_OFFSET(platform_clock_control_register_block_t, ccu2,       0x1000);


typedef volatile struct ATTR_PACKED {

	// Status register.
	struct  {
		uint32_t locked                        :  1;
		uint32_t is_free_running               :  1;
		uint32_t                               : 30;
	};

	// Control register.
	struct {
		uint32_t powered_down                   :  1;
		uint32_t bypassed                       :  1;
		uint32_t direct_input                   :  1;
		uint32_t direct_output                  :  1;
		uint32_t clock_enable                   :  1;
		uint32_t                                :  1;
		uint32_t set_free_running               :  1;
		uint32_t                                :  4;
		uint32_t block_during_frequency_changes :  1;
		uint32_t                                : 12;
		uint32_t source                         :  5;
		uint32_t                                :  3;
	};

	// M-divider register.
	union {

		// Provide the individual parts...
		struct {
			uint32_t m_divider_coefficient          : 17;
			uint32_t bandwidth_p                    :  5;
			uint32_t bandwidth_i                    :  6;
			uint32_t bandwidth_r                    :  4;
		};

		// ... and the whole register.
		uint32_t m_divider_encoded;
	};

	// NP-divider.
	union {

		// Provide the individual parts...
		struct {
			uint32_t p_divider_coefficient          :  7;
			uint32_t                                :  5;
			uint32_t n_divider_coefficient          : 10;
			uint32_t                                : 10;
		};

		// ... and the whole register.
		uint32_t np_divider_encoded;
	};

} platform_peripheral_pll_t;


/**
 * Structure representing the clock generation registers.
 */
typedef volatile struct ATTR_PACKED {
	RESERVED_WORDS(5);

	// Frequency monitor
	struct {
		uint32_t reference_ticks_remaining : 9;
		uint32_t observed_clock_ticks      : 14;
		uint32_t measurement_active        : 1;
		uint32_t source_to_measure         : 5;
		uint32_t                           : 3;
	} frequency_monitor;

	// XTAL oscillator control
	struct {
		uint32_t disabled                  : 1;
		uint32_t bypass                    : 1;
		uint32_t is_high_frequency         : 1;
		uint32_t                           : 29;
	} xtal_control;

	// USB high-speed PLL.
	platform_peripheral_pll_t pll_usb;

	// The PLL audio is a peripheral PLL with another register added for the fractional divider.
	struct {

		// The core of the register is the main PLL itself...
		platform_peripheral_pll_t core;

		// ... and an add-on represents our fractional divider.
		struct {
			uint32_t fractional_divider : 22;
			uint32_t                    : 10;
		};

	} pll_audio;

	// Main PLL1.
	struct {

		/* Status register. */
		struct {
			uint32_t locked                         : 1;
			uint32_t                                : 31;
		};

		/* Control register. */
		struct {
			uint32_t power_down                     : 1;
			uint32_t bypass_pll_entirely            : 1;
			uint32_t                                : 4;
			uint32_t use_pll_feedback               : 1;
			uint32_t bypass_output_divider          : 1;
			uint32_t output_divisor_P               : 2;
			uint32_t                                : 1;
			uint32_t block_during_frequency_changes : 1;
			uint32_t input_divisor_N                : 2;
			uint32_t                                : 2;
			uint32_t feedback_divisor_M             : 8;
			uint32_t source                         : 5;
			uint32_t                                : 3;
		};

	} pll1;


	// Integer divisor devices.
	platform_base_clock_register_t idiva;
	platform_base_clock_register_t idivb;
	platform_base_clock_register_t idivc;
	platform_base_clock_register_t idivd;
	platform_base_clock_register_t idive;

	// Core base clocks.
	platform_base_clock_register_t safe;
	platform_base_clock_register_t usb0;
	platform_base_clock_register_t periph;
	platform_base_clock_register_t usb1;
	platform_base_clock_register_t m4;
	platform_base_clock_register_t spifi;
	platform_base_clock_register_t spi;
	platform_base_clock_register_t phy_rx;
	platform_base_clock_register_t phy_tx;
	platform_base_clock_register_t apb1;
	platform_base_clock_register_t apb3;
	platform_base_clock_register_t lcd;
	platform_base_clock_register_t adchs;
	platform_base_clock_register_t sdio;
	platform_base_clock_register_t ssp0;
	platform_base_clock_register_t ssp1;
	platform_base_clock_register_t uart0;
	platform_base_clock_register_t uart1;
	platform_base_clock_register_t uart2;
	platform_base_clock_register_t uart3;
	platform_base_clock_register_t out;
	RESERVED_WORDS(4);
	platform_base_clock_register_t audio;
	platform_base_clock_register_t out0;
	platform_base_clock_register_t out1;
} platform_clock_generation_register_block_t;

ASSERT_OFFSET(platform_clock_generation_register_block_t, frequency_monitor, 0x14);
ASSERT_OFFSET(platform_clock_generation_register_block_t, xtal_control,      0x18);
ASSERT_OFFSET(platform_clock_generation_register_block_t, pll_usb,           0x1c);
ASSERT_OFFSET(platform_clock_generation_register_block_t, pll_audio,         0x2c);
ASSERT_OFFSET(platform_clock_generation_register_block_t, pll1,              0x40);
ASSERT_OFFSET(platform_clock_generation_register_block_t, idiva,             0x48);
ASSERT_OFFSET(platform_clock_generation_register_block_t, audio,             0xc0);



/**
 * Helpful initialization macros.
 */
#define CGU_OFFSET(name) offsetof(platform_clock_generation_register_block_t, name)
#define CCU_OFFSET(name) offsetof(platform_clock_control_register_block_t, name)



/**
 * Return a reference to the LPC43xx's CCU block.
 */
platform_clock_control_register_block_t *get_platform_clock_control_registers(void);


/**
 * Return a reference to the LPC43xx's CGU block.
 */
platform_clock_generation_register_block_t *get_platform_clock_generation_registers(void);


/**
 * Turns on the clock for a given peripheral.
 * (Clocks for this function are found in the clock control register block.)
 *
 * @param clock The clock to enable.
 */
void platform_enable_branch_clock(platform_branch_clock_register_t *clock, bool divide_by_two);


/**
 * Turns off the clock for a given peripheral (not branch clocks).
 * (Clocks for this function are found in the clock control register block.)
 *
 * @param clock The clock to enable.
 */
void platform_disable_clock(platform_branch_clock_register_t *clock);


/**
 * Set up the source for a provided generic base clock.
 *
 * @param clock The base clock to be configured.
 * @param source The clock source for the given clock.
 *
 * @return 0 on success, or an error number on failure
 */
int platform_select_base_clock_source(platform_base_clock_register_t *clock, clock_source_t source);


/**
 * Function that determines the primary clock input, which determines which
 * root clock (i.e. which oscillator) is accepted to drive the primary clock source.
 *
 * A default implementation is provided, but end software can override this
 * to select a different alternate soruce e.g. programmatically.
 */
clock_source_t platform_determine_primary_clock_input(void);


/**
 * Function that determines the primary clock source, which will drive
 * most of the major clocking sections of the device.
 *
 * A default implementation is provided, but end software can override this
 * to select a different alternate soruce e.g. programmatically.
 */
clock_source_t platform_determine_primary_clock_source(void);


/**
 * Initialize all of the system's clocks -- typically called by the crt0 as part of the platform setup.
 */
void platform_initialize_clocks(void);


/**
 * Initialize any clocks that need to be brought up at the very beginning
 * of system initialization. Typically called by the crt0 as part of the platform setup.
 */
void platform_initialize_early_clocks(void);


/**
 * Uses the LPC43xx's internal frequency monitor to detect the frequency of the given clock source.
 * If trying to determine the internal clock frequency, the external oscillator must be up, as it will
 * be used as the refernece clock.
 *
 * @param source The source to be meausred.
 * @return The relevant frequency, in Hz, or 0 if the given clock is too low to measure.
 */
uint32_t platform_detect_clock_source_frequency(clock_source_t clock_to_detect);


/**
 * @return a string containing the given clock source's name
 */
const char *platform_get_clock_source_name(clock_source_t source);


/**
 * @returns the frequency of the provided branch clock, in Hz.
 */
uint32_t platform_get_branch_clock_frequency(platform_branch_clock_t *clock);

/**
 * @return the configured parent source for the given clock, or 0 if the clock doesn't appear to have one
 */
clock_source_t platform_get_parent_clock_source(clock_source_t source);

#endif
