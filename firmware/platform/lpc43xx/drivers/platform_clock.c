/*
 * This file is part of libgreat
 *
 * LPC43xx clock setup / control drivers
 */

#include <errno.h>

#include <debug.h>
#include <drivers/timer.h>
#include <drivers/platform_clock.h>

#include <toolchain.h>


// Don't try to bring up any clock more than five times.
static const uint32_t platform_clock_max_bringup_attempts = 5;

/**
 * Base address for the LPC43xx Clock Generation Address.
 */
#define CGU_BASE_ADDRESS  (0x40050000UL)

/**
 * Base address for the LPC43xx Clock Generation Address.
 */
#define CCU_BASE_ADDRESS  (0x40051000UL)


/**
 * Constants to make configuration easier.
 */
#define  HZ (1UL)
#define KHZ (1000UL)
#define MHZ (1000000UL)


/**
 * Helper macros for getting quick references ot the system base / branch clocks.
 * Intended only for use in this driver; external sources should manually grab the offset in the CCU/CGU.
 */
#define BASE_CLOCK(name)   (platform_base_clock_t *)(CGU_BASE_ADDRESS + CGU_OFFSET(name))
#define BRANCH_CLOCK(name) (platform_branch_clock_t *)(CCU_BASE_ADDRESS + CCU_OFFSET(name))


// Forward declarations.
static int platform_handle_dependencies_for_clock_source(clock_source_t source);
static void platform_handle_clock_source_frequency_change(clock_source_t source);
static uint32_t platform_get_clock_source_frequency(clock_source_t source);
void platform_handle_base_clock_frequency_change(platform_base_clock_t *clock);
clock_source_t platform_get_physical_clock_source(clock_source_t source);
static int platform_ensure_main_xtal_is_up(void);
static void platform_soft_start_cpu_clock(void);
static int platform_bring_up_main_pll(uint32_t frequency);
static char *platform_get_base_clock_name(platform_base_clock_t *base);
static platform_base_clock_t *platform_base_clock_for_divider(clock_source_t source);
static int platform_bring_up_audio_pll(void);
static int platform_bring_up_usb_pll(void);

// Set to true once we're finished with early initialization.
// (We can't do some things until early init completes.)
static bool platform_early_init_complete;


/**
 * Data structure represneting the configuration for each active clock source.
 */
typedef struct {

	// True iff the clock source is currently enabled.
	bool enabled;

	// The expected frequency of the clock source, in Hz.
	// If this is set to 0, any input frequency will be acceptable.
	uint32_t frequency;

	// The actual / measured frequency of the clock source, in Hz.
	uint32_t frequency_actual;

	// If this is a generated clock source, this field indicates the source for the generated clock.
	// Otherwise, this field is meaningless.
	clock_source_t source;

	// Set to true once the given clock has been brought up, so we can avoid setting it up again.
	bool up_and_okay;

	// Counts the number of total failures to bring this clock up.
	bool failure_count;

} platform_clock_source_configuration_t;


/**
 * Active configurations for each of the system's clock sources.
 */
ATTR_WEAK platform_clock_source_configuration_t platform_clock_source_configurations[CLOCK_SOURCE_COUNT] = {

	// Slow oscillators, both external (RTC) and internal (IRC).
	[CLOCK_SOURCE_32KHZ_OSCILLATOR]    = { .frequency = 32 * KHZ },
	[CLOCK_SOURCE_INTERNAL_OSCILLATOR] = { .frequency = 12 * MHZ, .frequency_actual = 12 * MHZ, .up_and_okay = true},

	// Clock inputs -- these accept clocks directly on a GPIO pin.
	[CLOCK_SOURCE_ENET_RX_CLOCK]       = { .frequency = 50 * MHZ },
	[CLOCK_SOURCE_ENET_TX_CLOCK]       = { .frequency = 50 * MHZ },
	[CLOCK_SOURCE_GP_CLOCK_INPUT]      = {},

	// Main clock oscillator.
	[CLOCK_SOURCE_XTAL_OSCILLATOR]     = { .frequency = 12 * MHZ, .frequency_actual = 12 * MHZ },

	// Derived clocks -- including PLLs and dividiers.
	[CLOCK_SOURCE_PLL0_USB]            = { .frequency = 480 * MHZ, .source = CLOCK_SOURCE_PRIMARY_INPUT },
	[CLOCK_SOURCE_PLL0_AUDIO]          = {},
	[CLOCK_SOURCE_PLL1]                = { .frequency = 204 * MHZ, .source = CLOCK_SOURCE_PRIMARY_INPUT },
	[CLOCK_SOURCE_DIVIDER_A_OUT]       = {},
	[CLOCK_SOURCE_DIVIDER_B_OUT]       = {},
	[CLOCK_SOURCE_DIVIDER_C_OUT]       = {},
	[CLOCK_SOURCE_DIVIDER_D_OUT]       = {},
	[CLOCK_SOURCE_DIVIDER_E_OUT]       = {}
};


/**
 * Data structure describing the relationship between a base clock and its
 * subordinate "branch" clocks, as well as configuration defaults for each clock.
 */
typedef struct {

	char *name;

	// The location of the relevant base clock in the CGU;
	// represented as an offset into the CGU block.
	uintptr_t cgu_offset;

	// The location of the relevant block of CCU registers.
	uintptr_t ccu_region_offset;

	// The span of the CCU base. For convenience, a value of 0 is
	// equivalent to a minimum span of 0x100.
	uintptr_t ccu_region_span;

	// Initial CGU clock settings.
	clock_source_t source;
	uint8_t divisor;

	// Freuqency, in Hz.
	uint32_t frequency;

	// Indicates that a given clock is unused, and thus should never be brought up.
	bool unused;

	// Indicates that a given clock has no possible configuration.
	bool cannot_be_configured;

	// Indicates that the provided clock should not attempt to fall back to the internal oscillator.
	bool no_fallback;


} platform_base_clock_configuration_t;


/**
 * Platform configuration for each of the local clocks.
 * These specify the relationships between the various generated and derivative clocks.
 */
ATTR_WEAK platform_base_clock_configuration_t clock_configs[] = {
	{ .name = "idiva",  .cgu_offset = CGU_OFFSET(idiva), .source = CLOCK_SOURCE_PLL0_USB,      .divisor = 4 },
	{ .name = "idivb",  .cgu_offset = CGU_OFFSET(idivb), .source = CLOCK_SOURCE_DIVIDER_A_OUT, .divisor = 2 },
	{ .name = "idivc",  .cgu_offset = CGU_OFFSET(idivc) },
	{ .name = "idivd",  .cgu_offset = CGU_OFFSET(idivd) },
	{ .name = "idive",  .cgu_offset = CGU_OFFSET(idive) },
	{ .name = "safe",   .cgu_offset = CGU_OFFSET(safe), .cannot_be_configured = true },
	{ .name = "usb0",   .cgu_offset = CGU_OFFSET(usb0),   .ccu_region_offset = CCU_OFFSET(usb0),
			.source = CLOCK_SOURCE_PLL0_USB, .no_fallback = true },
	{ .name = "periph", .cgu_offset = CGU_OFFSET(periph), .ccu_region_offset = CCU_OFFSET(periph),
			.source = CLOCK_SOURCE_PRIMARY },
	{ .name = "usb1",   .cgu_offset = CGU_OFFSET(usb1),   .ccu_region_offset = CCU_OFFSET(usb1),
			.source = CLOCK_SOURCE_DIVIDER_B_OUT },
	{ .name = "m4",     .cgu_offset = CGU_OFFSET(m4),     .ccu_region_offset = CCU_OFFSET(m4),
			.ccu_region_span = 0x300, .source = CLOCK_SOURCE_PRIMARY, },
	{ .name = "spifi",  .cgu_offset = CGU_OFFSET(spifi),  .ccu_region_offset = CCU_OFFSET(spifi),
		.source = CLOCK_SOURCE_PRIMARY  },
	{ .name = "spi",    .cgu_offset = CGU_OFFSET(spi),    .ccu_region_offset = CCU_OFFSET(spi),
		.source = CLOCK_SOURCE_PRIMARY },
	{ .name = "phy_rx", .cgu_offset = CGU_OFFSET(phy_rx) },
	{ .name = "phy_tx", .cgu_offset = CGU_OFFSET(phy_tx) },
	{ .name = "apb1",   .cgu_offset = CGU_OFFSET(apb1),   .ccu_region_offset = CCU_OFFSET(apb1),
			.source = CLOCK_SOURCE_PRIMARY },
	{ .name = "apb3",   .cgu_offset = CGU_OFFSET(apb3),   .ccu_region_offset = CCU_OFFSET(apb3),
			.source = CLOCK_SOURCE_PRIMARY },
	{ .name = "lcd",    .cgu_offset = CGU_OFFSET(lcd) },
	{ .name = "adchs",  .cgu_offset = CGU_OFFSET(adchs),  .ccu_region_offset = CCU_OFFSET(adchs),
			.source = CLOCK_SOURCE_DIVIDER_B_OUT },
	{ .name = "sdio",   .cgu_offset = CGU_OFFSET(sdio),   .ccu_region_offset = CCU_OFFSET(sdio),
			.source = CLOCK_SOURCE_PRIMARY },
	{ .name = "ssp0",   .cgu_offset = CGU_OFFSET(ssp0),   .ccu_region_offset = CCU_OFFSET(ssp0),
			.source = CLOCK_SOURCE_PRIMARY },
	{ .name = "ssp1",   .cgu_offset = CGU_OFFSET(ssp1),   .ccu_region_offset = CCU_OFFSET(ssp1),
			.source = CLOCK_SOURCE_PRIMARY },
	{ .name = "uart0",  .cgu_offset = CGU_OFFSET(uart0),  .ccu_region_offset = CCU_OFFSET(usart0),
			.source = CLOCK_SOURCE_PRIMARY },
	{ .name = "uart1",  .cgu_offset = CGU_OFFSET(uart1),  .ccu_region_offset = CCU_OFFSET(uart1),
			.source = CLOCK_SOURCE_PRIMARY },
	{ .name = "uart2",  .cgu_offset = CGU_OFFSET(uart2),  .ccu_region_offset = CCU_OFFSET(usart2),
			.source = CLOCK_SOURCE_PRIMARY },
	{ .name = "uart3",  .cgu_offset = CGU_OFFSET(uart3),  .ccu_region_offset = CCU_OFFSET(usart3),
			.source = CLOCK_SOURCE_PRIMARY },
	{ .name = "out",    .cgu_offset = CGU_OFFSET(out)   },
	{ .name = "out0",   .cgu_offset = CGU_OFFSET(out0)  },
	{ .name = "out1",   .cgu_offset = CGU_OFFSET(out1)  },
	{ .name = "audio",  .cgu_offset = CGU_OFFSET(audio), .ccu_region_offset = CCU_OFFSET(audio),
		.source = CLOCK_SOURCE_PRIMARY_INPUT },

	// Sentinel; indicates the end of our collection.
	{}

};

/**
 * Full collection of branch clocks. Allows us to iterate over each branch clock to perform e.g. maintenance tasks.
 */
static platform_branch_clock_t *all_branch_clocks[] = {
	BRANCH_CLOCK(apb3.bus), BRANCH_CLOCK(apb3.i2c1), BRANCH_CLOCK(apb3.dac), BRANCH_CLOCK(apb3.adc0),
	BRANCH_CLOCK(apb3.adc1), BRANCH_CLOCK(apb3.can0), BRANCH_CLOCK(apb1.bus), BRANCH_CLOCK(apb1.motocon_pwm),
	BRANCH_CLOCK(apb1.i2c0), BRANCH_CLOCK(apb1.i2s), BRANCH_CLOCK(apb1.can1), BRANCH_CLOCK(spifi),
	BRANCH_CLOCK(m4.bus), BRANCH_CLOCK(m4.spifi), BRANCH_CLOCK(m4.gpio), BRANCH_CLOCK(m4.lcd),
	BRANCH_CLOCK(m4.ethernet), BRANCH_CLOCK(m4.usb0), BRANCH_CLOCK(m4.emc), BRANCH_CLOCK(m4.sdio), BRANCH_CLOCK(m4.dma),
	BRANCH_CLOCK(m4.core), BRANCH_CLOCK(m4.sct), BRANCH_CLOCK(m4.usb1), BRANCH_CLOCK(m4.emcdiv),
	BRANCH_CLOCK(m4.flasha), BRANCH_CLOCK(m4.flashb), BRANCH_CLOCK(m4.m0app), BRANCH_CLOCK(m4.adchs),
	BRANCH_CLOCK(m4.eeprom), BRANCH_CLOCK(m4.wwdt), BRANCH_CLOCK(m4.usart0), BRANCH_CLOCK(m4.uart1),
	BRANCH_CLOCK(m4.ssp0), BRANCH_CLOCK(m4.timer0), BRANCH_CLOCK(m4.timer1), BRANCH_CLOCK(m4.scu),
	BRANCH_CLOCK(m4.creg), BRANCH_CLOCK(m4.ritimer), BRANCH_CLOCK(m4.usart2), BRANCH_CLOCK(m4.usart3),
	BRANCH_CLOCK(m4.timer2), BRANCH_CLOCK(m4.timer3), BRANCH_CLOCK(m4.ssp1), BRANCH_CLOCK(m4.qei),
	BRANCH_CLOCK(periph.bus), BRANCH_CLOCK(periph.core), BRANCH_CLOCK(periph.sgpio), BRANCH_CLOCK(usb0),
	BRANCH_CLOCK(usb1), BRANCH_CLOCK(spi), BRANCH_CLOCK(adchs), BRANCH_CLOCK(audio), BRANCH_CLOCK(usart3),
	BRANCH_CLOCK(usart2), BRANCH_CLOCK(uart1), BRANCH_CLOCK(usart0), BRANCH_CLOCK(ssp1), BRANCH_CLOCK(ssp0),
	BRANCH_CLOCK(sdio)
};

/**
 * Full collection of base clocks. Allows us to iterate over each base clock to perform e.g. maintenance tasks.
 */
 static platform_base_clock_t *all_base_clocks[] = {
	BASE_CLOCK(idiva), BASE_CLOCK(idivb), BASE_CLOCK(idivc), BASE_CLOCK(idivd), BASE_CLOCK(idive), BASE_CLOCK(safe),
	BASE_CLOCK(usb0), BASE_CLOCK(periph), BASE_CLOCK(usb1), BASE_CLOCK(m4), BASE_CLOCK(spifi), BASE_CLOCK(spi),
	BASE_CLOCK(phy_rx), BASE_CLOCK(phy_tx), BASE_CLOCK(apb1), BASE_CLOCK(apb3), BASE_CLOCK(lcd), BASE_CLOCK(adchs),
	BASE_CLOCK(sdio), BASE_CLOCK(ssp0), BASE_CLOCK(ssp1), BASE_CLOCK(uart0), BASE_CLOCK(uart1), BASE_CLOCK(uart2),
	BASE_CLOCK(uart3), BASE_CLOCK(out), BASE_CLOCK(audio), BASE_CLOCK(out0), BASE_CLOCK(out1)
 };

/**
 * Names for each of the branch clocks. Indexes are the same as all_branch_clock indexes.
 */
static const char *branch_clock_names[] = {
	"apb3.bus", "apb3.i2c1", "apb3.dac", "apb3.adc0", "apb3.adc1", "apb3.can0", "apb1.bus", "apb1.motocon_pwm",
	"apb1.i2c0", "apb1.i2s", "apb1.can1", "spifi", "m4.bus", "m4.spifi", "m4.gpio", "m4.lcd", "m4.ethernet", "m4.usb0",
	"m4.emc", "m4.sdio", "m4.dma", "m4.core", "m4.sct", "m4.usb1", "m4.emcdiv", "m4.flasha", "m4.flashb", "m4.m0app",
	"m4.adchs", "m4.eeprom", "m4.wwdt", "m4.usart0", "m4.uart1", "m4.ssp0", "m4.timer0", "m4.timer1", "m4.scu",
	"m4.creg", "m4.ritimer", "m4.usart2", "m4.usart3", "m4.timer2", "m4.timer3", "m4.ssp1", "m4.qei", "periph.bus",
	"periph.core", "periph.sgpio", "usb0", "usb1", "spi", "adchs", "audio", "usart3", "usart2", "uart1", "usart0",
	"ssp1", "ssp0", "sdio"
};


/**
 * Return a reference to the LPC43xx's CCU block.
 */
platform_clock_control_register_block_t *get_platform_clock_control_registers(void)
{
	return (platform_clock_control_register_block_t *)CCU_BASE_ADDRESS;
}


/**
 * Return a reference to the LPC43xx's CGU block.
 */
platform_clock_generation_register_block_t *get_platform_clock_generation_registers(void)
{
	return (platform_clock_generation_register_block_t *)CGU_BASE_ADDRESS;
}


/**
 * Convert an offset into the CGU register block into a base-clock object.
 *
 * @param cgu_offset The offset into the CGU register bank.
 */
static platform_base_clock_t *platform_get_base_clock_from_cgu_offset(uintptr_t cgu_offset)
{
	if (!cgu_offset) {
		return NULL;
	}

	return (platform_base_clock_register_t *)(CGU_BASE_ADDRESS + cgu_offset);
}


/**
 * Convert an offset into the CGU register block into a branch-clock object.
 *
 * @param cgu_offset The offset into the CCU register bank.
 */
static platform_branch_clock_t *platform_get_branch_clock_from_ccu_offset(uintptr_t ccu_offset)
{
	if (!ccu_offset) {
		return NULL;
	}

	return (platform_branch_clock_register_t *)(CCU_BASE_ADDRESS + ccu_offset);
}


/**
 * Fetches the register that controls the parent clock for the given peripheral clock.
 */
static const platform_base_clock_configuration_t *platform_find_config_for_branch_clock(platform_branch_clock_t *clock)
{
	// Iterator.
	const platform_base_clock_configuration_t *config;

	// Figure out the byte offset of the given clock entity.
	uintptr_t ccu_offset = (uintptr_t)clock - CCU_BASE_ADDRESS;

	// Search through our possible base clocks until we hit a sentinel, or find our result.
	for (config = clock_configs; config->name; ++config) {
		uintptr_t ccu_region_span = config->ccu_region_span ? config->ccu_region_span : 0x100;
		uintptr_t ccu_region_max  = config->ccu_region_offset + ccu_region_span;

		// If this entry doesn't have a CCU offset, it doesn't have an associated branch clock,
		// and thus isn't of interest of us.
		if (!config->ccu_region_offset) {
			continue;
		}

		// If the relevant clock's offset is in the relevant region, return the corresponding base clock.
		if ((ccu_offset >= config->ccu_region_offset) && (ccu_offset < ccu_region_max)) {
			return config;
		}
	}

	// If we didn't find a base, return NULL.
	return NULL;
}

/**
 * Fetches the register that controls the parent clock for the given peripheral clock.
 */
static platform_base_clock_configuration_t *platform_find_config_for_base_clock(platform_base_clock_t *clock)
{
	// Iterator.
	platform_base_clock_configuration_t *config;

	// Figure out the byte offset of the given clock entity.
	uintptr_t cgu_offset = (uintptr_t)clock - CGU_BASE_ADDRESS;

	// Search through our possible base clocks until we hit a sentinel, or find our result.
	for (config = clock_configs; config->name; ++config) {

		// If the relevant clock's offset is in the relevant region, return the corresponding base clock.
		if (cgu_offset == config->cgu_offset) {
			return config;
		}
	}

	// If we didn't find a config, return NULL.
	return NULL;
}


/**
 * Fetches the clock that controls the bus the given peripheral is on.
 */
static platform_branch_clock_t *platform_get_bus_clock(platform_branch_clock_register_t *clock)
{
	platform_branch_clock_t *bus_clock;

	// Find the configuration entry that covers the given branch clock.
	const platform_base_clock_configuration_t *config = platform_find_config_for_branch_clock(clock);

	// If we didn't find a configuration entry, return NULL.
	if (!config) {
		return NULL;
	}

	// Otherwise, get a reference to the bus clock.
	bus_clock = platform_get_branch_clock_from_ccu_offset(config->ccu_region_offset);

	// Special cases: some perpiherals are connected directly, and thus don't have a bus clock.
	// We internally represent these clocks as being their own bus clock; but it doesn't make sense
	// to return that here -- we don't e.g. want to turn on a peripheral in order to turn on itself.
	if (bus_clock == clock) {
		return NULL;
	}

	return bus_clock;
}


/**
 * @return a string containing the given clock source's name
 */
const char *platform_get_clock_source_name(clock_source_t source)
{
	switch(source) {
		case CLOCK_SOURCE_32KHZ_OSCILLATOR:    return "32kHz oscillator";
		case CLOCK_SOURCE_INTERNAL_OSCILLATOR: return "internal oscillator";
		case CLOCK_SOURCE_ENET_RX_CLOCK:       return "ethernet rx clock";
		case CLOCK_SOURCE_ENET_TX_CLOCK:       return "ethernet tx clock";
		case CLOCK_SOURCE_GP_CLOCK_INPUT:      return "clock input";
		case CLOCK_SOURCE_XTAL_OSCILLATOR:     return "external crystal oscillator";
		case CLOCK_SOURCE_PLL0_USB:			   return "USB PLL";
		case CLOCK_SOURCE_PLL0_AUDIO:		   return "audio PLL";
		case CLOCK_SOURCE_PLL1:				   return "core PLL";
		case CLOCK_SOURCE_DIVIDER_A_OUT:       return "divider-A";
		case CLOCK_SOURCE_DIVIDER_B_OUT:       return "divider-B";
		case CLOCK_SOURCE_DIVIDER_C_OUT:	   return "divider-C";
		case CLOCK_SOURCE_DIVIDER_D_OUT:       return "divider-D";
		case CLOCK_SOURCE_DIVIDER_E_OUT:       return "divider-E";
		case CLOCK_SOURCE_PRIMARY:             return "primary clock";
		case CLOCK_SOURCE_PRIMARY_INPUT:       return "primary input clock";
		default:                               return "unknown source";
	}
}


/**
 * Ensures the provided clock is active and can be used.
 */
int platform_enable_base_clock(platform_base_clock_register_t *base)
{
	int rc;
	platform_base_clock_t value;

	// Identify the relevant configuration for the given base clock.
	platform_base_clock_configuration_t *config = platform_find_config_for_base_clock(base);

	// If this clock cannot be configured, we'll assume it's already okay.
	// TODO: validate the configuration settings?
	if (config->cannot_be_configured) {
		return 0;
	}

	// Switch the base clock to its relevant clock source.
	rc = platform_handle_dependencies_for_clock_source(config->source);
	if (rc && !config->no_fallback) {
		pr_warning("failed to bring up source %s for base clock %s; falling back to internal oscillator!\n",
			platform_get_clock_source_name(config->source), platform_get_base_clock_name(base));
		config->source = CLOCK_SOURCE_INTERNAL_OSCILLATOR;
	}
	else if (rc) {
		pr_warning("failed to bring up source %s for base clock %s; trying to continue anyway.\n",
			platform_get_clock_source_name(config->source), platform_get_base_clock_name(base));
	}

	// Finally, ensure the clock is powered up.
	value.power_down = 0;
	value.block_during_changes = 0;
	value.source = config->source;
	value.divisor = 0;
	base->all_bits = value.all_bits;

	return 0;
}


void platform_disable_base_clock(platform_base_clock_register_t *base)
{
	const platform_base_clock_configuration_t *config = platform_find_config_for_base_clock(base);

	if (config->cannot_be_configured) {
		return;
	}

	// TODO: provide an complement to platform_handle_dependencies_for_clock_source
	// that allows us to disable any dependencies for this clock source if necessary.

	// Power down the base clock.
	base->power_down = true;
}


/**
 * Fetches the base clock for the given branch clock.
 */
static platform_base_clock_register_t *platform_get_clock_base(platform_branch_clock_register_t *clock)
{
	// Iterator.
	const platform_base_clock_configuration_t *config = platform_find_config_for_branch_clock(clock);

	// If we didn't find a configuration entry, return NULL.
	if (!config) {
		return NULL;
	}

	// Otherwise, return the base clock.
	return platform_get_base_clock_from_cgu_offset(config->cgu_offset);
}


/**
 * @return a string describing the given base clock
 */
static char *platform_get_base_clock_name(platform_base_clock_t *base)
{
	// Find the base clock configuration, which also has the base clock's name.
	const platform_base_clock_configuration_t *config = platform_find_config_for_base_clock(base);

	// If we found a config, return its name; otherwise return a default string.
	if (config && config->name) {
		return config->name;
	} else {
		return "unknown clock";
	}
}


/**
 * Returns true iff the given base clock is in use.
 *
 * Searches manually for branches that depend on the given base clock,
 * so this can be used even for generated clocks.
 */
bool platform_clock_source_in_use(clock_source_t source)
{
	platform_clock_generation_register_block_t *cgu = get_platform_clock_generation_registers();

	// Search all of the branch clocks for any clocks that depend on us.
	for (unsigned i = 0; i < ARRAY_SIZE(all_branch_clocks); ++i) {
		platform_branch_clock_t *branch = all_branch_clocks[i];
		platform_base_clock_t *base = platform_get_clock_base(branch);

		// If we can't find a base clock for this branch, it doesn't depend on us.
		if (!base) {
			continue;
		}

		// If this clock or its source are off, it doesn't depend on us.
		if (base->power_down || branch->current.disabled) {
			continue;
		}

		// If this clock is derived from a source other than us, it doesn't depend on us.
		if (base->source != source) {
			continue;
		}

		return true;
	}

	// Search all of the base clocks for any _base_ clocks that depend on us.
	for (unsigned i = 0; i < ARRAY_SIZE(all_base_clocks); ++i) {
		platform_base_clock_t *base = all_base_clocks[i];

		// If this clock isn't based on us, it doesn't depened on us.
		if (base->source != source) {
			continue;
		}

		// If this clock is off, it doesn't depend on us.
		if (base->power_down) {
			continue;
		}

		return true;
	}

	// Check our PLLs to see if any of them depend on us.
	if (!cgu->pll1.power_down && (cgu->pll1.source == source)) {
		return true;
	}
	if (!cgu->pll_usb.powered_down && (cgu->pll_usb.source == source)) {
		return true;
	}
	if (!cgu->pll_audio.core.powered_down && (cgu->pll_audio.core.source == source)) {
		return true;
	}

	// If none of the conditions above were met, we're not in use!
	return false;
}


/**
 * @returns true iff the given base clock is in use
 */
static bool platform_base_clock_in_use(platform_base_clock_register_t *base)
{
	platform_clock_control_register_block_t    *ccu = get_platform_clock_control_registers();

	// Find the CGU offset, which we'll use to look up the appropriate register.
	uintptr_t cgu_offset = (uintptr_t)base - CGU_BASE_ADDRESS;

	switch(cgu_offset) {
		case CGU_OFFSET(idiva): return platform_clock_source_in_use(CLOCK_SOURCE_DIVIDER_A_OUT);
		case CGU_OFFSET(idivb): return platform_clock_source_in_use(CLOCK_SOURCE_DIVIDER_B_OUT);
		case CGU_OFFSET(idivc): return platform_clock_source_in_use(CLOCK_SOURCE_DIVIDER_C_OUT);
		case CGU_OFFSET(idivd): return platform_clock_source_in_use(CLOCK_SOURCE_DIVIDER_D_OUT);
		case CGU_OFFSET(idive): return platform_clock_source_in_use(CLOCK_SOURCE_DIVIDER_E_OUT);

		// The "safe" clock is always available, intentionally.
		case CGU_OFFSET(safe):
			return true;

		// For most other clocks, return whether the hardware reports them as driving a branch clock.
		case CGU_OFFSET(usb0):   return ccu->usb0_needed;
		case CGU_OFFSET(periph): return ccu->periph_needed;
		case CGU_OFFSET(usb1):   return ccu->usb1_needed;
		case CGU_OFFSET(m4):     return ccu->m4_needed;
		case CGU_OFFSET(spifi):  return ccu->spifi_needed;
		case CGU_OFFSET(spi):    return ccu->spi_needed;
		case CGU_OFFSET(apb1):   return ccu->apb1_needed;
		case CGU_OFFSET(apb3):   return ccu->apb3_needed;
		case CGU_OFFSET(ssp0):   return ccu->ssp0_needed;
		case CGU_OFFSET(ssp1):   return ccu->ssp1_needed;
		case CGU_OFFSET(uart0):  return ccu->uart0_needed;
		case CGU_OFFSET(uart1):  return ccu->uart1_needed;
		case CGU_OFFSET(uart2):  return ccu->uart2_needed;
		case CGU_OFFSET(uart3):  return ccu->uart3_needed;

		// FIXME: For now, assume that output clocks are always on.
		// We can probably develop a better logic for this later.
		case CGU_OFFSET(audio):  return true;
		case CGU_OFFSET(out):    return true;
		case CGU_OFFSET(out0):   return true;
		case CGU_OFFSET(out1):   return true;

		// FIXME: these should return whether m4.ethernet is enabled
		case CGU_OFFSET(phy_rx): return true;
		case CGU_OFFSET(phy_tx): return true;

		// FIXME: this should return whether their relevant branch clocks are enabled.
		// These usually bave both their own branch clocks and a clock on the m4.
		case CGU_OFFSET(lcd):    return true;
		case CGU_OFFSET(adchs):  return true;
		case CGU_OFFSET(sdio):   return true;
	}

	// If we didn't find a relevant clock, assume the clock is necessary.
	return true;
}


/**
 * @return a string containing the given clock source's name
 */
static const char *platform_get_branch_clock_name(platform_branch_clock_t *clock)
{
	for (unsigned i = 0; i < ARRAY_SIZE(all_branch_clocks); ++i) {
		if (all_branch_clocks[i] == clock)
			return branch_clock_names[i];
	}

	return "unknown branch clock";
}


/**
 * Disables the given base clock iff it's no longer used; e.g. if it no longer
 * drives any active branch clocks.
 */
void platform_disable_base_clock_if_unused(platform_base_clock_t *base)
{
	// Check to see if the base clock is in use.
	// If it is, we don't need to disable it; bail out.
	if (platform_base_clock_in_use(base)) {
		return;
	}

	pr_debug("clock: base clock %s no longer in use; disabling.\n", platform_get_base_clock_name(base));

	// Otherwise, disable the base clock.
	platform_disable_base_clock(base);
}


/**
 * Updates our internal notion of the IRC frequency -- usually as a result of measuring a more
 * accurate clock source, such as the system's external crystal oscillator.
 */
static void platform_calibrate_irc_frequency(uint32_t frequency)
{
	platform_clock_source_configurations[CLOCK_SOURCE_INTERNAL_OSCILLATOR].frequency_actual = frequency;
	platform_handle_clock_source_frequency_change(CLOCK_SOURCE_INTERNAL_OSCILLATOR);
}


/**
 * @returns The system's internal oscillator frequency; to the best of our knowledge.
 */
static uint32_t platform_get_irc_frequency(void)
{
	return platform_clock_source_configurations[CLOCK_SOURCE_INTERNAL_OSCILLATOR].frequency_actual;
}


/**
 * @returns True iff the given clock is ticking.
 */
static bool validate_clock_source_is_ticking(clock_source_t source)
{
	const uint32_t timeout = 1000;

	platform_clock_generation_register_block_t *cgu = get_platform_clock_generation_registers();
	uint32_t time_base = get_time();

	// Create a measurement of the given clock source that should complete very quickly IFF the given
	// clock is up.
	cgu->frequency_monitor.source_to_measure = source;
	cgu->frequency_monitor.reference_ticks_remaining = 1;

	// Trigger our measurement, and wait for it to complete.
	cgu->frequency_monitor.measurement_active = 1;
	while (cgu->frequency_monitor.measurement_active) {

		// If we exceed our timeout, cancel the measurement, and abort.
		if (get_time_since(time_base) > timeout) {
			cgu->frequency_monitor.measurement_active = 0;
			return false;
		}
	}

	// If the measurement completed, it must be ticking; continue.
	return true;
}



/**
 * Performs a single iteration of frequency measurement using the frequency-monitor hardware.
 * Mostly used by this API's measurement function, platform_detect_clock_source_frequency;
 * this algorithm mostly makes sense in its context.
 *
 * @param observed_ticks_max The maximum number of observed-clock ticks that should be able to
 *		occur before the counters are halted.
 * @param observed_ticks_max The maximum number of reference-clock ticks that should be able to
 *		occur before the counters are halted.
 *
 * @param use_reference_timeframe If true, the time spent counting will be returned as a number of reference
 *	 clock ticks; if false, it will be returened as a number of observed-clock ticks.
 */
static uint32_t platform_run_frequency_measurement_iteration(uint32_t observed_ticks_max,
		uint32_t measurement_period_max, bool use_reference_timeframe)
{
	platform_clock_generation_register_block_t *cgu = get_platform_clock_generation_registers();

	const uint32_t observed_tick_register_saturation_point = 0x3FFF;

	// Normally, the observed ticks only stop the measurement if the counter saturates -- so, to impose our maximum
	// we'll need to initialize the counter with a value such that it saturates after `observed_ticks_max` ticks.
	// So, we'll figure out how many ticks we want to happen _until_ the saturation point.
	uint32_t initial_observed_ticks = observed_tick_register_saturation_point - observed_ticks_max;

	// Set the reference clock value to their maximum values.
	// The reference clock counts down, while the selected clock counts up,
	// so the extremes are the highest and lowest posisble values, respectively.
	cgu->frequency_monitor.reference_ticks_remaining = measurement_period_max;
	cgu->frequency_monitor.observed_clock_ticks      = initial_observed_ticks;

	// Trigger our measurement, and wait for it to complete.
	cgu->frequency_monitor.measurement_active = 1;
	while (cgu->frequency_monitor.measurement_active);

	// Return the value we managed to count to with our selected clock.
	if (use_reference_timeframe) {
		// It's possible we terminated early by hitting our maximu observed ticks;
		// so compensate by subtracting the number of ticks remaining.
		return measurement_period_max - cgu->frequency_monitor.reference_ticks_remaining;
	} else {
		// If we added an initial value to the observred counter to reduce the ticks until
		// the counter saturates, we'll need to remove them before we return the total ticks that occurred.
		return cgu->frequency_monitor.observed_clock_ticks - initial_observed_ticks;
	}

}



/**
 * @return true iff the last frequency measurement run completed a full measurement period;
 *		this will return false if it aborted early due to hitting its maxium number of observed-clock-ticks
 */
uint32_t platform_last_frequency_measurement_period_ticks_left_over(void)
{
	platform_clock_generation_register_block_t *cgu = get_platform_clock_generation_registers();
	return cgu->frequency_monitor.reference_ticks_remaining;
}


/**
 * @return true iff the last frequency measurement run completed a full measurement period;
 *		this will return false if it aborted early due to hitting its maxium number of observed-clock-ticks
 */
bool platform_last_frequency_measurement_period_completed(void)
{
	return platform_last_frequency_measurement_period_ticks_left_over() == 0;
}


/**
 * Uses the LPC43xx's internal frequency monitor to detect the frequency of the given clock source.
 * If trying to determine the internal clock frequency, the external oscillator must be up, as it will
 * be used as the refernece clock.
 *
 * This version will never use an integer divider; so it will be inaccurate for higher-frequency clocks.
 *
 * @param source The source to be meausred.
 * @return The relevant frequency, in Hz, or 1 if the given clock is too low to measure (a stopped clock
 *    will correctly return 0 Hz).
 */
uint32_t platform_detect_clock_source_frequency_directly(clock_source_t clock_to_detect)
{
	double resultant_frequency, resultant_ratio;

	// Maximum values for our counters -- determined by the bit size of the counters in the frequency_monitor
	// registers.
	const uint32_t observed_ticks_max = 0x3FFF;
	const uint32_t measurement_period_max = 0x1FF;

	volatile uint32_t observed_ticks, measurement_period;

	platform_clock_generation_register_block_t *cgu = get_platform_clock_generation_registers();
	clock_source_t clock_to_measure = clock_to_detect;

	// We can't calibrate the internal oscillator (IRC) against itself;
	// so we'll compare against the external XTAL, and use that to detect the IRC frequency.
	if (clock_to_detect == CLOCK_SOURCE_INTERNAL_OSCILLATOR) {
		clock_to_measure = CLOCK_SOURCE_XTAL_OSCILLATOR;
	}
	// Otherwise, calibrate the internal frequency against the XTAL first.
	// (The XTAL is more accurate; this will help to null out any drift due to e.g. temperature.)
	else {
		uint32_t measured = platform_detect_clock_source_frequency_directly(CLOCK_SOURCE_INTERNAL_OSCILLATOR);

		// If we managed a calibration, continue.
		if (measured) {
			platform_calibrate_irc_frequency(measured);
		}
	}

	// Special case: if the given clock source isn't ticking, bail out immediately with a frequency of 0 Hz.
	if (!validate_clock_source_is_ticking(clock_to_measure)) {
		return 0;
	}

	// Set the frequency monitor to measure the appropriate clock.
	cgu->frequency_monitor.source_to_measure = clock_to_measure;

	// This monitor works by measuring the number of clock ticks that occur on the "observed clock"
	// over a defined "measurement period", which is measured by allowing a number of _reference clock_ cycles to pass.
	// We'll start off with the longest posisble period, and decrease the measurement period _iff_ it allows
	// us to get more accuracy (see below).
	measurement_period = measurement_period_max;

	// Try a first iteration of our measurement, allowing the measurement to go on for as long as possible.
	observed_ticks = platform_run_frequency_measurement_iteration(observed_ticks_max, measurement_period_max, false);

	// If we made it through the measurement period without seeing even a single tick on the clock we're observing,
	// the clock is too slow for us to measure.  We can't measure clocks slower than ~24kHz, as we don't have enough
	// bits in our period timer to make the measurement period any longer. :(
	if (observed_ticks == 0) {

		// Indicate that this clock is too slow to be measured.
		return 0;
	}

	// We now have an initial reading, but it's possible we can improve on this reading's accuracy. The observed
	// clock and reference clock are effecively racing until either the measurment clock reached its maximum value
	// or the measurement period elapses.
	//
	// If we stopped at the end of the measurement period, then we have a potential source of noise: we don't know for
	// sure that we measured _an integer number_ of measurement clock periods, as our measurement period could have
	// ended anywhere in our observed clock's cycle.
	if (platform_last_frequency_measurement_period_completed())
	{
		// Luckily, we can fix this: we can decrease the measurement period until we see fewer ticks.
		while(platform_run_frequency_measurement_iteration(observed_ticks, measurement_period--, false) == observed_ticks)

		// We'll count our measurement period to be equal to the _last_ period at which we saw the same amount of ticks
		// -- this is the shortest amount of time in which we can see the observed amount of ticks -- and thus as close
		// as we can measure to a span that contians an integer number of observed-clock periods.

		// Since we stopped decreasing the measurement period length _just after_ the number of ticks changed, we'll
		// go back by one to find the last value before the change.
		measurement_period++;
	}

	// We also have another source of error: if we stopped due to reaching the most observed ticks we can count,
	// then we likely stopped before the full measurement period has elapsed -- so this measurement doesn't correspond
	// to the full span of time.
	else {
		if (!observed_ticks) {
			pr_error("error: internally inconsistent frequency readings; the source seems unstable or too fast!\n");
			return 0;
		}

		// Since we stopped decreasing the total number of observed-ticks _just after_ they affected our measurement
		// period, we'll go back by one to find the actual amount of ticks that occur in our measurement period.
		observed_ticks++;
	}

	// We now have an as-accurate-as-possible ratio of (observed-ticks)-to-(measurement-period) -- which is effectively
	// the ratio of our observed and reference clock frequencies. We can use that to compute the relevant clock
	// frequency.
	if (clock_to_detect != clock_to_measure) {
		resultant_ratio     = (double)measurement_period / (double)observed_ticks;
		resultant_frequency = platform_clock_source_configurations[clock_to_measure].frequency * resultant_ratio;
	} else {
		resultant_ratio     = (double)observed_ticks / (double)measurement_period;
		resultant_frequency = platform_get_irc_frequency() * resultant_ratio;
	}

	return (uint32_t)resultant_frequency;
}

/**
 * Attempts to find an integer divider that's not in use.
 * @returns the given divider output, or CLOCK_SOURCE_NONE if none is availble.
 */
clock_source_t platform_find_free_integer_divider(void)
{
	// Prefer later-numbered clock dividers first; they're less liekly to be used.
	clock_source_t integer_dividers[] =
	{ CLOCK_SOURCE_DIVIDER_E_OUT, CLOCK_SOURCE_DIVIDER_D_OUT, CLOCK_SOURCE_DIVIDER_C_OUT,
	  CLOCK_SOURCE_DIVIDER_B_OUT, CLOCK_SOURCE_DIVIDER_A_OUT};

	// Search each of our integer dividers for one that's not in use.
	for (unsigned i = 0; i < ARRAY_SIZE(integer_dividers); ++i) {
		clock_source_t candidate_source = integer_dividers[i];

		// If the clock source is currently unused, return it.
		if (!platform_clock_source_in_use(candidate_source)) {
			return candidate_source;
		}
	}

	// If we didn't have one, abort.
	return CLOCK_SOURCE_NONE;
}

/**
 * Attempts to measure the frequency of the USB PLL, in Hz.
 * @returns the frequency in Hz, or 0 if the frequency could not be measured.
 */
static uint32_t platform_detect_usb_pll_frequency(void)
{
	// Only divisor A can be based off of the USB PLL. We'll need to check to see if we can use it.
	platform_clock_generation_register_block_t *cgu = get_platform_clock_generation_registers();
	platform_base_clock_t *divider = &cgu->idiva;
	uint32_t divided_frequency;

	// If the divider isn't set up to divide the USB PLL, return a low-precision measurement.
	//platform_handle_dependencies_for_clock_source(CLOCK_SOURCE_DIVIDER_A_OUT);
	if ((divider->source != CLOCK_SOURCE_PLL0_USB) || divider->power_down) {
		return platform_detect_clock_source_frequency_directly(CLOCK_SOURCE_PLL0_USB);
	}

	// If it is set up, measure its output, and then multiply away the divider.
	divided_frequency = platform_detect_clock_source_frequency_directly(CLOCK_SOURCE_DIVIDER_A_OUT);
	return  divided_frequency * (divider->divisor + 1);
}



/**
 * Uses the LPC43xx's internal frequency monitor to detect the frequency of the given clock source.
 * This version is allowed to consume an integer divider in order to more accurately measure higher-frequency clocks,
 * and will do so if necessary.
 *
 * @param source The source to be meausred.
 * @param source The integer divider to be consumed; or CLOCK_SOURCE_NONE to automatically detect one, if possible.
 * @return The relevant frequency, in Hz, or 1 if the given clock is too low to measure (a stopped clock
 *    will correctly return 0 Hz).
 */
uint32_t platform_detect_clock_source_frequency_via_divider(clock_source_t clock_to_detect, clock_source_t divider)
{
	platform_base_clock_t *divider_clock;
	platform_base_clock_t original_state;

	// If the clock is above 240 MHz, it'll be divided by four before our final measurement.
	const uint32_t divider_cutoff = 240 * MHZ;
	const uint32_t scale_factor = 4;

	// Get an initial measurement, which will determine if we need to re-compute using our divider.
	uint32_t frequency = platform_detect_clock_source_frequency_directly(clock_to_detect);

	// If this frequency is below our divider cutoff, we don't need to harness a divider.
	// Return it immediately.
	if (frequency < divider_cutoff) {
		return frequency;
	}

	// Sanity check to make sure we didn't make it here with our slow clock, whcih should be guaranteed to be < 120 MHz.
	if (clock_to_detect == CLOCK_SOURCE_INTERNAL_OSCILLATOR) {
		pr_error("error: measured the internal oscillator at %" PRIu32 "  Hz; that makes no sense!\n", frequency);
		return 0;
	}

	// If this is the USB PLL, this can only drive divider A. We'll need to take special steps.
	if (clock_to_detect == CLOCK_SOURCE_PLL0_USB) {
;		return platform_detect_usb_pll_frequency();
	}

	// If the dividier is CLOCK_SOURCE_NONE
	if (divider == CLOCK_SOURCE_NONE) {
		divider = platform_find_free_integer_divider();
	}

	// Get the base clock object that corresponds to the given divider.
	divider_clock = platform_base_clock_for_divider(divider);

	// If we don't have a divider to use, return the frequency directly.
	if (!divider_clock) {
		pr_warning("warning: trying to meausre a high-frequency clock, but all integer dividers are in use!\n");
		pr_warning("         The accuracy of the relevant measurement will be reduced.\n");

		return frequency;
	}

	// Store the state of the divider we're going to use, so we can restore it.
	original_state = *divider_clock;

	// Otherwise, enable the given divider, with our relevant scale factor.
	divider_clock->power_down = false;
	divider_clock->source = clock_to_detect;
	divider_clock->block_during_changes = 1;
	divider_clock->divisor = scale_factor - 1;

	// Measure the _divided_ version of our clock, and multiply our result by the amount we divided by.
	frequency = platform_detect_clock_source_frequency_directly(divider) * scale_factor;

	// Restore the state of the relevant divider.
	*divider_clock = original_state;
	return frequency;
}


/**
 * Uses the LPC43xx's internal frequency monitor to detect the frequency of the given clock source.
 * If trying to determine the internal clock frequency, the external oscillator must be up, as it will
 * be used as the refernece clock.
 *
 * @param source The source to be meausred.
 * @return The relevant frequency, in Hz, or 1 if the given clock is too low to measure (a stopped clock
 *    will correctly return 0 Hz).
 */
uint32_t platform_detect_clock_source_frequency(clock_source_t clock_to_detect)
{
	return platform_detect_clock_source_frequency_via_divider(clock_to_detect, CLOCK_SOURCE_NONE);
}


/**
 * Verifies the frequency of a given clock source; this also sets our
 * known actual frequency for the given source.
 *
 * @return 0 on success, or an error code on failure
 */
static int platform_verify_source_frequency(clock_source_t source)
{
	// Get a reference to the configuration for the given source.
	platform_clock_source_configuration_t *config = &platform_clock_source_configurations[source];

	// Measure the clock's actual frequency.
	config->frequency_actual = platform_detect_clock_source_frequency(source);
	pr_debug("clock: clock %s measured at %" PRIu32 " Hz\n", platform_get_clock_source_name(source), config->frequency_actual);

	// If the given source is 0 Hz and it's not supposed to be, return an error.
	// TODO: valdiate that this is close enough to the specified frequency, instead
	if (config->frequency && !config->frequency_actual) {
		pr_error("error: clock: clock %s (%d) did not come up correctly! (actual frequency %" PRIu32
		" Hz vs expected %" PRIu32 " Hz)\n",
				platform_get_clock_source_name(source), source, config->frequency_actual, config->frequency);
		config->up_and_okay = false;
		return EIO;
	}

	// TODO: if this is the XTAL oscillator, should we just modify the actual to be the specified, assuming we're
	// in the span? it's much more accurate than our IRC, and we should be calibrating accordingly
	config->up_and_okay = true;
	return 0;
}

/**
 * @return true iff the given clock source is already up, running, and configured
 */
static bool platform_clock_source_is_configured(clock_source_t source)
{
	return platform_clock_source_configurations[source].up_and_okay;
}


/**
 * @return true iff the given clock source is already up, running, and configured
 */
static bool platform_clock_source_is_configured_at_frequency(clock_source_t source, uint32_t frequency)
{
	// If this clock isn't configured for the appropriate frequency, this can't be right. Return false.
	if (platform_clock_source_configurations[source].frequency != frequency) {
		return false;
	}
	return platform_clock_source_is_configured(source);
}



/**
 * Ensures that the system's primary clock oscillator is up, enabling us to switch
 * to the more accurate crystal oscillator.
 */
static int platform_ensure_main_xtal_is_up(void)
{
	platform_clock_generation_register_block_t *cgu = get_platform_clock_generation_registers();

	// If the XTAL is already configured, we're done!
	if (platform_clock_source_is_configured(CLOCK_SOURCE_XTAL_OSCILLATOR)) {
		return 0;
	}

	// Ensure we're not in bypass.
	cgu->xtal_control.bypass = 0;

	// Per the datasheet, the bypass and enable executions must not be modified
	// in the same write -- so we use a barrier to ensure the writes stay separate.
	__sync_synchronize();

	// Enable the crystal oscillator.
	cgu->xtal_control.disabled = 0;

	// Wait 250us.
	delay_us(250UL);

	// XXX
	delay_us(250UL * 10);

	// Success!
	return platform_verify_source_frequency(CLOCK_SOURCE_XTAL_OSCILLATOR);
}


static int platform_ensure_rtc_xtal_is_up(void)
{
	// FIXME: Implement bringing up the RTC clock input.
	return ENOSYS;
}



static int platform_route_clock_input(clock_source_t source)
{
	// FIXME: Implement bringing external clocks to the relevant sources (GP_CLKIN and RTC, when bypassed).
	(void) source;
	return ENOSYS;
}

/**
 * @returns the base clock associated with the given integer clock divider
 */
static platform_base_clock_t *platform_base_clock_for_divider(clock_source_t source)
{
	platform_clock_generation_register_block_t *cgu = get_platform_clock_generation_registers();

	switch (source) {
		case CLOCK_SOURCE_DIVIDER_A_OUT: return &cgu->idiva;
		case CLOCK_SOURCE_DIVIDER_B_OUT: return &cgu->idivb;
		case CLOCK_SOURCE_DIVIDER_C_OUT: return &cgu->idivc;
		case CLOCK_SOURCE_DIVIDER_D_OUT: return &cgu->idivd;
		case CLOCK_SOURCE_DIVIDER_E_OUT: return &cgu->idive;
		default: return NULL;
	}
}


/**
 * Brings up the clock divider that drives a given clock source.
 *
 * @source The clock source corresponding to the relevant clock divider.
 * @return 0 on success, or an error number on failure
 */
static int platform_bring_up_clock_divider(clock_source_t source)
{
	int rc;

	// Find the base clock that coresponds tro the given divider.
	platform_base_clock_t *clock = platform_base_clock_for_divider(source);
	platform_base_clock_t value;

	//  If the given divider has already been configured, this is a trivial success.
	if (platform_clock_source_is_configured(source)) {
		return 0;
	}

	 // Apply the relevant divisor to the base clock.
	const platform_base_clock_configuration_t *config = platform_find_config_for_base_clock(clock);

	// ... and finally, enable the given clock.
	rc = platform_handle_dependencies_for_clock_source(config->source);
	if (rc) {
		return rc;
	}

	// Build the value to apply, and then apply it all at once, so we don't leave the write mid-configuration.
	value.power_down = 0;
	value.block_during_changes = 0;
	value.source = config->source;
	value.divisor = config->divisor - 1;
	clock->all_bits = value.all_bits;

	return 0;
}

/**
 * Configures and brings up the source necessary to use the generated clock, per our local configuration array.
 *
 * @param source The clock generator to be configured.
 */
static uint32_t set_source_for_generated_clock(clock_source_t source)
{
	int rc;

	platform_clock_generation_register_block_t *cgu = get_platform_clock_generation_registers();
	platform_clock_source_configuration_t *config = &platform_clock_source_configurations[source];
	platform_clock_source_configuration_t *parent_config;

	clock_source_t parent_clock = platform_get_physical_clock_source(config->source);
	parent_config =  &platform_clock_source_configurations[parent_clock];

	// Ensure that the parent clock is up.
	rc = platform_handle_dependencies_for_clock_source(parent_clock);
	if (rc) {
		pr_critical("critical: failed to bring up source %s for the main PLL; falling back to internal oscillator",
			platform_get_clock_source_name(parent_clock));
		parent_clock = config->source = CLOCK_SOURCE_INTERNAL_OSCILLATOR;
	}

	// Set the actual source itself.
	switch (source) {

		// Main PLLs.
		case CLOCK_SOURCE_PLL1:
			cgu->pll1.source = parent_clock;
			break;

		default:
			pr_warning("warning: cannot set source for clock %s (%d) as we don't know how!\n",
					platform_get_clock_source_name(source), source);
			return 0;
	}

	// Return the clock frequency for the source clock, or 0 if an error occurred.
	if (parent_config->frequency) {
		return parent_config->frequency;
	} else {
		return platform_get_clock_source_frequency(parent_clock);
	}
}


/**
 *
 */
static int platform_configure_main_pll_parameters(uint32_t target_frequency, uint32_t input_frequency)
{
	platform_clock_generation_register_block_t *cgu = get_platform_clock_generation_registers();

	const uint32_t input_divisor_max = 3;
	const uint32_t input_high_bound  = 25 * MHZ;
	const uint32_t cco_low_bound     = 156 * MHZ;

	uint32_t input_divisor = 1;
	uint32_t output_divisor = 0;
	uint32_t multiplier, rounding_offset;

	// If the input frequency is too high, try to divide it down to something acceptable.
	while (input_frequency > input_high_bound) {
		input_divisor++;
		input_frequency /= 2;
	}

	// If the necessary divider to reach an acceptable input freuqency is more than we can handle,
	// fail out. TODO: someday, it might be nice to automatically drive PLL1 off an integer divider?
	if (input_divisor > input_divisor_max) {
		pr_error("error: cannot drive PLL1 from a %" PRIu32 " Hz clock, which is too fast!\n", input_frequency);
		pr_error("       (you may want to drive PLL1 from an integer divider)\n");
		return EIO;
	}

	// If the target frequency is too low for our PLL to synthesize using its CCO, increase our target frequency,
	// but compensate by increasing our output divider.
	while (target_frequency < cco_low_bound) {
		pr_info("pll1: target frequency %" PRIu32 " Hz < CCO_min; doubling to %" PRIu32 " Hz and compensating with post-divider\n",
				target_frequency, target_frequency * 2);
		output_divisor++;
		target_frequency *= 2;
	}

	// We can configure the PLL in either integer or non-integer mode by determining whether we use the output
	// or oscillator clock to drive the PLL feedback. Using the output clock ("integer mode") gives us a more stable
	// (lower jitter) clock, but using the output clock ("non-integer") gives us more granularity in frequency
	// selection.
	//
	// For now, we'll allow non-integr modes, but we'll want to reconsider this to save power? TODO: do so!
	cgu->pll1.use_pll_feedback = 0;

	// Determine the multiplier for the PLL.
	// We offset the target frequency first by half of the input frequency to round nicely.
	rounding_offset = (input_frequency / 2);
	multiplier = (target_frequency + rounding_offset) / input_frequency;

	if (output_divisor) {
		pr_debug("pll1: computed integer-mode parameters: N: %" PRIu32 " M: %" PRIu32 " P: %" PRIu32 " for an input clock of %" PRIu32 " Hz\n",
			input_divisor - 1	, multiplier - 1, output_divisor - 1, input_frequency);
	} else {
		pr_debug("pll1: computed direct-mode: N: %" PRIu32 " M: %" PRIu32 " for an input clock of %" PRIu32 " Hz\n",
			input_divisor - 1, multiplier - 1, input_frequency);
	}

	// Program the PLL's various dividers, including the M-divider, which divides the PLL feedback path.
	// (Dividing the feedback path means the PLL will need to push the CCO higher to compensate, so it effectively
	// acts as a multiplier. See the LPC datasheet and any PLL documentation for theory info. W2AEW has a nice video.)
	cgu->pll1.feedback_divisor_M = multiplier - 1;
	cgu->pll1.input_divisor_N    = input_divisor - 1;

	// If we have an output divisor, use and program the output divisor.
	if (output_divisor) {
		cgu->pll1.output_divisor_P      = output_divisor - 1;
		cgu->pll1.bypass_output_divider = 0;
	}
	// Otherwise, bypass the output divisor and output the CCO frequency directly.
	else {
		cgu->pll1.bypass_output_divider = 1;
	}

	return 0;
}

/**
 */
static void platform_soft_start_cpu_clock(void)
{
	int rc;

	// Per the user manual, we need to soft start if the relevant frequency is  >= 110 MHz [13.2.1.1].
	// This means holding the relevant base clock at half-frequency for 50uS.
	const uint32_t soft_start_cutoff = 110 * MHZ;
	const uint32_t soft_start_duration = 50;

	platform_clock_generation_register_block_t *cgu = get_platform_clock_generation_registers();

	// Identify the clock source for the CPU, which will determine if we have to soft-start.
	const platform_base_clock_configuration_t *config = platform_find_config_for_base_clock(&cgu->m4);
	clock_source_t parent_clock = platform_get_physical_clock_source(config->source);

	// And read the source clock's target frequency.
	uint32_t source_frequency = platform_clock_source_configurations[parent_clock].frequency;

	// If this clock is going to run at a frequency low enough that we don't have to soft-start it,
	// we'll abort here and let the normal configuration bring the clock up.
	if (source_frequency < soft_start_cutoff) {
		return;
	}

	// For now, we only support soft-starting off of PLL1.
	// TODO: support soft-starting via other clocks, perhaps using an integer divider?
	if (parent_clock != CLOCK_SOURCE_PLL1) {
		pr_warning("warning: not able to soft-switch the CPU to source %s (%d); system may be unstable.\n",
				platform_get_clock_source_name(parent_clock), parent_clock);
		return;
	}

	pr_debug("clock: soft-switching the main CPU clock to %" PRIu32 " Hz\n", source_frequency);

	// First, ensure the main CPU complex is running our safe, slow internal oscillator.
	cgu->m4.source = CLOCK_SOURCE_INTERNAL_OSCILLATOR;

	// Configure the main PLL to produce the target frequency -- this is essentially the mode we _want_ to run in.
	// This configures the core PLL to come up in the state we want.
	rc = platform_bring_up_main_pll(source_frequency);
	if (rc) {
		return;
	}

	// If we're currently bypassing the output divider, turning the divider
	// on (and to its least setting) achieves a trivial divide-by-two.
	if (cgu->pll1.bypass_output_divider) {
		cgu->pll1.output_divisor_P      = 0;
		cgu->pll1.bypass_output_divider = 0;
	} else {
		cgu->pll1.output_divisor_P++;
	}
	while (!cgu->pll1.locked);

	// Set the main CPU clock to our halved PLL...
	cgu->m4.source = parent_clock;
	platform_handle_base_clock_frequency_change(&cgu->m4);

	// ... and hold it there for our soft-start period.
	pr_debug("clock: CPU is now running from %s\n", platform_get_clock_source_name(parent_clock));
	delay_us(soft_start_duration);

	// Undo our changes, bringing the PLL output back up to its full speed.
	if (cgu->pll1.output_divisor_P == 0) {
		cgu->pll1.bypass_output_divider = 1;
	} else {
		cgu->pll1.output_divisor_P--;
	}
	while (!cgu->pll1.locked);

	platform_handle_base_clock_frequency_change(&cgu->m4);
	pr_debug("clock: CPU is now running at our target speed of %" PRIu32 "\n", source_frequency);
}


/**
 * Bring up the system's main PLL at the desired frequency.
 *
 * @param frequency The desired frequency; max of 204 MHz.
 */
static int platform_bring_up_main_pll(uint32_t frequency)
{
	const uint32_t pll_lock_timeout = 1000000; // 1 second; this should probably be made tweakable

	// Store the bounds for the PLL's internal programmable current-controlled oscillator (CCO).
	const uint32_t input_low_bound  = 10 * MHZ;
	const uint32_t output_low_bound = 9750 * KHZ;
	const uint32_t cco_high_bound   = 320 * MHZ;

	platform_clock_generation_register_block_t *cgu = get_platform_clock_generation_registers();
	platform_clock_source_configuration_t *config = &platform_clock_source_configurations[CLOCK_SOURCE_PLL1];

	uint32_t input_frequency, time_base;
	int rc;

	// If the PLL is already configured, we're done!
	if (platform_clock_source_is_configured_at_frequency(CLOCK_SOURCE_PLL1, frequency)) {
		return 0;
	}

	if (config->failure_count > platform_clock_max_bringup_attempts)
	{
		pr_error("error: not trying to bring up main PLL; too many failures\n");
		return ETIMEDOUT;
	}

	// Update the clock configuration to match the provided frequency.
	config->up_and_okay = false;
	config->frequency = frequency;
	pr_debug("clock: configuring main PLL to run at %" PRIu32 " Hz.\n", frequency);

	// Validate our freuqency bounds.
	if (frequency > cco_high_bound) {
		pr_error("error: cannot program PLL1 to frequency %" PRIu32 "; this frequency is higher than is possible\n", frequency);
		pr_error("       (you may want to derive your clock from PLL0, which can generate higher frequencies)\n");
		return EINVAL;
	}
	if (frequency < output_low_bound) {
		pr_error("error: cannot program PLL1 to frequency %" PRIu32 "; this frequency is lower than is possible\n", frequency);
		pr_error("       (you may want to derive your clock from an integer divider based off of a PLL)\n");
		return EINVAL;
	}

	// Decouple ourselves from configuration of the clock, so we can adjust it without worrying about it blocking.
	cgu->pll1.block_during_frequency_changes = 0;

	// Set the source for the relevant PLL.
	input_frequency = set_source_for_generated_clock(CLOCK_SOURCE_PLL1);

	// Check to make sure the input frequency isn't too low.
	if (input_frequency < input_low_bound) {
		pr_error("error: cannot drive PLL1 from a %" PRIu32 " Hz clock; must be at least %" PRIu32 " Hz\n",
				input_frequency, input_low_bound);
		return EIO;
	}

	// Configure the PLL itself.
	rc = platform_configure_main_pll_parameters(frequency, input_frequency);
	if (rc) {
		return rc;
	}

	// Wait for a lock to occur.
	time_base = get_time();
	while (!cgu->pll1.locked) {
		if (get_time_since(time_base) > pll_lock_timeout) {
			pr_error("error: PLL lock timed out (attempt %d)!\n", config->failure_count);
			config->failure_count += 1;
			return ETIMEDOUT;
		}
	}

	// Verify that we produced an appropriate source frequency for the device.
	rc = platform_verify_source_frequency(CLOCK_SOURCE_PLL1);
	if (rc) {
		return rc;
	}

	platform_handle_clock_source_frequency_change(CLOCK_SOURCE_PLL1);
	return 0;
}

/**
 * @return an integer representing the likely-intended clock frequency for the primary input source, in MHz.
 */
static unsigned platform_identify_clock_frequency_mhz(clock_source_t source)
{
	const uint32_t rounding_factor = MHZ /2;

	// Get the input frequency of our main clock input.
	clock_source_t physical_source = platform_get_physical_clock_source(source);
	uint32_t frequency = platform_clock_source_configurations[physical_source].frequency;

	// Round the frequency to the nearest MHz and return.
	return (frequency + rounding_factor) / MHZ;
}

/**
 * Conigure the USB PLL to produce the freuqencies necessary to drive the platform's
 * various USB controllers.
 */
static int platform_bring_up_audio_pll(void)
{
	// TODO: Implement support for the audio PL.
	pr_error("error: clock: audio PLL support not yet implemeneted!");
	return ENOSYS;
}

/**
 * Conigure the USB PLL to produce the freuqencies necessary to drive the platform's
 * various USB controllers.
 */
static int platform_bring_up_usb_pll(void)
{
	const uint32_t usb_pll_target = 480 * MHZ;
	unsigned source_frequency;

	// Pre-computed and encoded multiplier/divider constants for the USB PLL.
	// From datasheet table 152 (section 13.8.3).
	// TODO: replace this LUT with a computation, probably?
	const uint32_t m_divider_constants[] = {
		0x00000000, 0x073e56c9, 0x073e2dad, 0x0b3e34b1, // 0, 1, 2, 3 MHz
		0x0e3e7777, 0x0d326667, 0x0b2a2a66, 0x00000000, // 4, 5, 6, 7
		0x08206aaa, 0x00000000, 0x071a7faa, 0x00000000, // 8, 9, 10, 11,
		0x06167ffa, 0x00000000, 0x00000000, 0x05123fff, // 12, 13, 14, 15,
		0x04101fff, 0x00000000, 0x00000000, 0x00000000, // 16, 17, 18, 19
		0x040e03ff, 0x00000000, 0x00000000, 0x00000000, // 20, 21, 22, 23,
		0x030c00ff // 24
	};
	const uint32_t np_divider_constant = 0x00302062;

	// Time to wait for the USB PLL to lock up.
	const uint32_t pll_lock_timeout = 1000000; // 1 second; this should probably be made tweakable

	platform_clock_generation_register_block_t *cgu = get_platform_clock_generation_registers();
	uint32_t time_base;

	// Get the clock that should be the basis for our frequency.
	platform_clock_source_configuration_t *config = &platform_clock_source_configurations[CLOCK_SOURCE_PLL0_USB];

	// Ensure the relevant clock is up.
	int rc = platform_handle_dependencies_for_clock_source(config->source);
	if (rc) {
		pr_warning("critical: failed to bring up source %s for USB PLL; falling back to internal oscillator!\n",
			platform_get_clock_source_name(config->source));
		config->source = CLOCK_SOURCE_INTERNAL_OSCILLATOR;
	}

	// TODO: support frequencies that aren't simple integers.
	source_frequency = platform_identify_clock_frequency_mhz(config->source);

	// If the relevant clock is already up and okay, we're done!
	if(platform_clock_source_is_configured(CLOCK_SOURCE_PLL0_USB)) {
		return 0;
	}

	// For now, ensure that we're trying to program a supported PLL frequency.
	if (config->frequency != usb_pll_target) {
		pr_error("error: cannot currently configure USB PLLs to frequencies other than %" PRIu32, usb_pll_target);
		return EINVAL;
	}

	// Check to ensure we can produce the relevant clock.
	if ((source_frequency > 24) || (m_divider_constants[source_frequency] == 0)) {
		pr_error("error: pll0-usb: cannot currently generate a USB clock from %s running at %" PRIu32 "\n",
			platform_get_clock_source_name(config->source), platform_get_clock_source_frequency(config->source));
	}

	// Power off the PLL for configuration.
	cgu->pll_usb.powered_down = 1;
	cgu->pll_usb.block_during_frequency_changes = 0;

	// Configure the PLL's source.
	cgu->pll_usb.source = platform_get_physical_clock_source(config->source);

	// And apply the relevant PLL settings.
	cgu->pll_usb.m_divider_encoded = m_divider_constants[source_frequency];
	cgu->pll_usb.np_divider_encoded = np_divider_constant;

	// Set the PLL to simple direct-mode.
	cgu->pll_usb.direct_input = 1;
	cgu->pll_usb.direct_output = 1;
	cgu->pll_usb.clock_enable = 1;
	cgu->pll_usb.set_free_running = 0;

	// Turn the PLL on...
	cgu->pll_usb.powered_down = 0;

	// ... and wait for it to lock.
	time_base = get_time();
	while (!cgu->pll_usb.locked) {
		if (get_time_since(time_base) > pll_lock_timeout) {

			pr_error("error: PLL lock timed out (attempt %d)!\n", config->failure_count);
			config->failure_count += 1;

			return ETIMEDOUT;
		}
	}

	// If we got here, we should be live!
	cgu->pll_usb.bypassed = false;
	return platform_verify_source_frequency(CLOCK_SOURCE_PLL0_USB);
}


/**
 * Ensures that all hardware dependencies are met to use the provided clock source,
 * bringing up any dependencies as needed.
 *
 *
 *
 * @param source The source for which dependencies should be identified.
 * @return 0 on success, or an error code on failure.
 */
static int platform_handle_dependencies_for_clock_source(clock_source_t source)
{
	source = platform_get_physical_clock_source(source);

	switch(source) {

		// If the requisite source is an xtal, start it.
		case CLOCK_SOURCE_XTAL_OSCILLATOR:
			return platform_ensure_main_xtal_is_up();
		case CLOCK_SOURCE_32KHZ_OSCILLATOR:
			return platform_ensure_rtc_xtal_is_up();

		// If the source is a direct clock input, ensure we can access it.
		case CLOCK_SOURCE_ENET_RX_CLOCK:
		case CLOCK_SOURCE_ENET_TX_CLOCK:
		case CLOCK_SOURCE_GP_CLOCK_INPUT:
			return platform_route_clock_input(source);

		// If the source is one of our dividers, start it.
		case CLOCK_SOURCE_DIVIDER_A_OUT:
		case CLOCK_SOURCE_DIVIDER_B_OUT:
		case CLOCK_SOURCE_DIVIDER_C_OUT:
		case CLOCK_SOURCE_DIVIDER_D_OUT:
		case CLOCK_SOURCE_DIVIDER_E_OUT:
			return platform_bring_up_clock_divider(source);

		// If the clock source is based on the main PLL, bring it up.
		case CLOCK_SOURCE_PLL1:
			return platform_bring_up_main_pll(platform_clock_source_configurations[source].frequency);

		// If the clock source is based on a fast PLL, bring it up.
		case CLOCK_SOURCE_PLL0_USB:
			return platform_bring_up_usb_pll();
		case CLOCK_SOURCE_PLL0_AUDIO:
			return platform_bring_up_audio_pll();

		// The internal oscillator is always up, so we don't need to handle any
		// dependencies for it.
		case CLOCK_SOURCE_INTERNAL_OSCILLATOR:
			return 0;

		// If we've received another clock source, something's gone wrong.
		// fail out.
		default:
			pr_error("clock: clould not bring up clock #%d (%s) as we don't know how!\n",
					source, platform_get_clock_source_name(source));
			return ENODEV;
	}
}


/**
 * Handles any changes to a given clock source.
 */
static void platform_handle_clock_source_frequency_change(clock_source_t source)
{
	platform_clock_generation_register_block_t *cgu = get_platform_clock_generation_registers();
	const clock_source_t dividers[] =
	{ CLOCK_SOURCE_DIVIDER_A_OUT, CLOCK_SOURCE_DIVIDER_B_OUT, CLOCK_SOURCE_DIVIDER_C_OUT,
      CLOCK_SOURCE_DIVIDER_D_OUT, CLOCK_SOURCE_DIVIDER_E_OUT };

	// Notify any base clocks that depend on us of the change.
	for(unsigned i = 0; i < ARRAY_SIZE(all_base_clocks); ++i) {
		platform_base_clock_t *base = all_base_clocks[i];

		if (!base->power_down && (base->source == source)) {
				platform_handle_base_clock_frequency_change(base);
		}
	}

	// Notify the descendents of any sources that depend on us.
	if (!cgu->pll1.power_down && (cgu->pll1.source == source)) {
		platform_handle_clock_source_frequency_change(CLOCK_SOURCE_PLL1);
	}
	if (!cgu->pll_usb.powered_down && (cgu->pll_usb.source == source)) {
		platform_handle_clock_source_frequency_change(CLOCK_SOURCE_PLL0_USB);
	}
	if (!cgu->pll_audio.core.powered_down && (cgu->pll_audio.core.source == source)) {
		platform_handle_clock_source_frequency_change(CLOCK_SOURCE_PLL0_AUDIO);
	}

	// Notify any descendents of any dividers that depend on us.
	for(unsigned i = 0; i < ARRAY_SIZE(dividers); ++i) {
		platform_base_clock_t *base = platform_base_clock_for_divider(dividers[i]);

		if (!base->power_down && (base->source == source)) {
				platform_handle_base_clock_frequency_change(base);
		}
	}

	// TODO: allow downstream components to register monitors for clock sources
	// which should be notified, here!
}

/**
 * Handles any changes to a provided clock.
 */
void platform_handle_branch_clock_frequency_change(platform_branch_clock_t *clock)
{
	platform_clock_control_register_block_t *ccu = get_platform_clock_control_registers();

	// TODO: allow downstream components to register monitors for base clock changes
	// which should be notified, here!

	// FIXME: Don't hardcode this! This is just a shim until we have a proper callback system.
	if (clock == &ccu->m4.timer3) {
		handle_platform_timer_frequency_change();
	}

}

/**
 * Handles any changes to a provided clock.
 */
void platform_handle_base_clock_frequency_change(platform_base_clock_t *clock)
{

	// Notify any branch clocks that depend on us.
	for (unsigned i = 0; i < ARRAY_SIZE(all_branch_clocks); ++i) {
		platform_branch_clock_t *branch = all_branch_clocks[i];
		platform_base_clock_t *base = platform_get_clock_base(branch);\

		// FIXME: should this not notify base objects that are disabled?
		if (!base->power_down && (base == clock)) {
			platform_handle_branch_clock_frequency_change(branch);
		}
	}

	// TODO: allow downstream components to register monitors for base clock changes
	// which should be notified, here!
}



/**
 * Translates a given clock source into the correct physical clock source--
 * handling virtual clock sources such as CLOCK_SOURCE_PRIMARY.
 */
clock_source_t platform_get_physical_clock_source(clock_source_t source)
{

	if (source == CLOCK_SOURCE_PRIMARY) {
		if (platform_early_init_complete) {
			source = platform_determine_primary_clock_source();
		} else {
			source = CLOCK_SOURCE_INTERNAL_OSCILLATOR;
		}
	}
	if (source == CLOCK_SOURCE_PRIMARY_INPUT) {
		source = platform_determine_primary_clock_input();
	}

	return source;
}


/**
 * Set up the source for a provided generic base clock.
 *
 * @param clock The base clock to be configured.
 * @param source The clock source for the given clock.
 */
int platform_select_base_clock_source(platform_base_clock_t *clock, clock_source_t source)
{
	int rc = 0;

	// Special case: if we have a virtual source, replace the variable with the real clock source
	// behind it. see platform_determine_primary_clock_source() / platform_determine_primary_clock_input()
	// for information on how downstream software can influence the primary source selection.
	source = platform_get_physical_clock_source(source);

	// Before we can switch to the given source, ensure that we can use it.
	rc = platform_handle_dependencies_for_clock_source(source);
	if (rc) {
		pr_critical("critical: failed to bring up clock source %s (%d)! Falling back to internal oscillator.\n",
			platform_get_clock_source_name(source), rc);
		source = CLOCK_SOURCE_INTERNAL_OSCILLATOR;
	}

	clock->block_during_changes = 1;
	clock->source = source;

	// Notify any consumers of the change.
	platform_handle_base_clock_frequency_change(clock);
	return rc;
}

/**
 * @returns true iff the given branch clock is divideable
 */
static bool platform_branch_clock_is_divideable(platform_branch_clock_t *clock)
{
	const platform_branch_clock_t *divideable_clocks[] = {
		BRANCH_CLOCK(m4.emcdiv), BRANCH_CLOCK(m4.flasha), BRANCH_CLOCK(m4.flashb),
		BRANCH_CLOCK(m4.m0app), BRANCH_CLOCK(m4.adchs), BRANCH_CLOCK(m4.eeprom)
	};

	// Check to see if the clock is in our list of divideable clocks.
	for (unsigned i = 0; i < ARRAY_SIZE(divideable_clocks); ++i) {
		if (divideable_clocks[i] == clock) {
			 return true;
		}
	}

	// If it's not, then it's not divideable.
	return false;
}


/**
 * Turns on the clock for a given peripheral.
 * (clocks for this function are found in the clock control register block.)
 *
 * @param clock The clock to enable.
 */
void platform_enable_branch_clock(platform_branch_clock_register_t *clock, bool divide_by_two)
{
	int rc;

	// Try to find each of the clocks that this perpiheral might depend on.
	platform_base_clock_register_t *base  = platform_get_clock_base(clock);
	platform_branch_clock_register_t *bus = platform_get_bus_clock(clock);

	// If we've found either of the clocks we depend on, enable them.
	if (base) {
		rc = platform_enable_base_clock(base);
		if (rc) {
			pr_warning("warning: failed to set up base clock for branch %s\n", platform_get_branch_clock_name(clock));
		}
	}
	if (bus) {
		platform_enable_branch_clock(bus, false);
	}

	// Zero out the advanced clock configuration options.
	clock->control.disable_when_bus_transactions_complete = 0;
	clock->control.wake_after_powerdown = 0;

	// If we're dividing by two, mark the divisor.
	if (platform_branch_clock_is_divideable(clock)) {
		clock->control.divisor = divide_by_two ? 1 : 0;
	}

	// Finally, enable the given clock.
	clock->control.enable = 1;
}


/**
 * @returns true iff the provided branch clock is critical and must remain on.
 */
bool platform_branch_clock_must_remain_on(platform_branch_clock_register_t *clock)
{
	// List of critical clocks we must never turn off.
	platform_branch_clock_register_t *critical_clocks[] = { BRANCH_CLOCK(m4.bus), BRANCH_CLOCK(m4.core) };

	// Check to see if the given clock is in our list of critical clocks.
	for (unsigned i = 0; i < ARRAY_SIZE(critical_clocks); ++i) {
		if (clock == critical_clocks[i]) {
			return true;
		}
	}

	// If it wasn't, then allow it to be disabled.
	return false;
}


/**
 * Turns off the clock for a given peripheral.
 * (clocks for this function are found in the clock control register block.)
 *
 * @param clock The clock to disable.
 */
void platform_disable_branch_clock(platform_branch_clock_register_t *clock)
{
	// Try to find the base_clock that owns the given clock.
	// this is the internal source that drives the relevant peripheral's clock.
	platform_base_clock_register_t *base = platform_get_clock_base(clock);

	// If this clock must remain on, never disable it.
	if (platform_branch_clock_must_remain_on(clock)) {
		return;
	}

	pr_debug("clock: disabling branch clock %s (%p)\n", platform_get_branch_clock_name(clock), clock);

	// Per the datasheet, disabling the clock should happen as two steps:
	// - we should set auto-disable-when-not-clocked, and then
	// - as a separate write, we should clear the enable bit.
	// we use a full barrier to ensure these writes aren't merged.
	clock->control.disable_when_bus_transactions_complete = 1;
	clock->control.wake_after_powerdown = 1;
	__sync_synchronize();
	clock->control.enable = 0;

	// If this branch clock has a parent clock, we'll disable it iff it's no longer
	// used. This allows us to save power.
	if (base) {
		platform_disable_base_clock_if_unused(base);
	}
}


/**
 * Default function that determines the primary clock source, which will drive
 * most of the major clocking sections of the device.
 */
ATTR_WEAK clock_source_t platform_determine_primary_clock_source(void)
{
	return CLOCK_SOURCE_PLL1;
}


/**
 * Function that determines the primary clock input, which determines which
 * root clock (i.e. which oscillator) is accepted to drive the primary clock source.
 */
ATTR_WEAK clock_source_t platform_determine_primary_clock_input(void)
{
	return CLOCK_SOURCE_XTAL_OSCILLATOR;
}



/**
 * @returns the frequency of the given clock source, in Hz.
 */
static uint32_t platform_get_clock_source_frequency(clock_source_t source)
{
	// Get a quick reference to the clock source's current state.
	platform_clock_source_configuration_t *config;

	// Get the configuration for the relevant clock source, ensuring we're always
	// dealing with a physical clock source.
	source = platform_get_physical_clock_source(source);
	config = &platform_clock_source_configurations[source];

	// If we don't have an actual frequency, attempt to measure one.
	if (config->frequency_actual == 0) {

		// If we can use our measurement hardware, do so.
		if (platform_early_init_complete) {
			pr_debug("clock: unknown frequency for source %s (%d); attempting to measure\n",
					platform_get_clock_source_name(source), source);

			platform_verify_source_frequency(source);
			pr_debug("clock: frequency meausred at %" PRIu32 " Hz\n", config->frequency_actual);
		}

		// Otherwise, just assume the relevant frequency.
		else {
			return config->frequency;
		}
	}

	// Return the final actual frequency for the given source.
	return config->frequency_actual;
}



/**
 * Returns the divider for the given base clock. For most of the base clocks,
 * there's no divider, and the divider is thus always 1. For the integer divider
 * clocks, this can be a dynamic number..
 */
static uint32_t platform_base_clock_get_divisor(platform_base_clock_t *clock)
{
	// List of clocks that have a divisor.
	platform_base_clock_t *dividable_clocks[] = {
		BASE_CLOCK(idiva),
		BASE_CLOCK(idivb),
		BASE_CLOCK(idivc),
		BASE_CLOCK(idivd),
		BASE_CLOCK(idive)
	};

	// Check to see if the given clock is in our list of clocks with dividers...
	for (unsigned i = 0; i < ARRAY_SIZE(dividable_clocks); ++i) {

		// .. if it is, grab its divisor.
		if (clock == dividable_clocks[i]) {
			return clock->divisor + 1;
		}

	}

	// If it wasn't, then its divisor is always '1'.
	return 1;
}


/**
 * @returns the frequency of the provided base clock, in Hz.
 */
uint32_t platform_get_base_clock_frequency(platform_base_clock_t *clock)
{
	// Find the frequency of the clock source, and our local divisor.
	uint32_t source_frequency = platform_get_clock_source_frequency(clock->source);
	uint32_t divisor = platform_base_clock_get_divisor(clock);

	// Return the relevant clock frequency.
	return source_frequency / divisor;
}

/**
 * @returns the frequency of the provided branch clock, in Hz.
 */
uint32_t platform_get_branch_clock_frequency(platform_branch_clock_t *clock)
{
	uint32_t base_frequency;
	uint32_t divisor = 1;

	// Find the base clock off of which the given clock is based.
	platform_base_clock_register_t *base = platform_get_clock_base(clock);

	// If we couldn't find one, we can't figure out this clock's frequency. Abort.
	if (!base) {
		return 0;
	}

	if (platform_branch_clock_is_divideable(clock)) {
		divisor = clock->control.current_divisor + 1;
	}

	// Find the frequency of our base clock.
	base_frequency = platform_get_base_clock_frequency(base);

	// Finally, return our base frequency, factoring in our clock's divisor.
	return base_frequency / divisor;
}


/**
 * @returns the clock source that drives the given branch clock
 */
clock_source_t platform_get_branch_clock_source(platform_branch_clock_t *clock)
{
	// Find the base clock off of which the given clock is based.
	platform_base_clock_register_t *base = platform_get_clock_base(clock);

	// If we couldn't find one, we can't figure out this clock's frequency. Abort.
	if (!base) {
		return platform_get_physical_clock_source(CLOCK_SOURCE_PRIMARY);
	}

	// Return the base clock's source.
	return base->source;
}


/**
 * @return the configured parent source for the given clock, or 0 if the clock doesn't appear to have one
 */
clock_source_t platform_get_parent_clock_source(clock_source_t source)
{
	return platform_clock_source_configurations[source].source;
}



/**
 * Initialize any clocks that need to be brought up at the very beginning
 * of system initialization.
 */
void platform_initialize_early_clocks(void)
{
	platform_clock_generation_register_block_t *cgu = get_platform_clock_generation_registers();
	platform_early_init_complete = false;

	// Switch the system clock onto the 12MHz internal oscillator for early initialization.
	// This gives us a stable clock to work with to set up everything else.
	platform_select_base_clock_source(&cgu->m4, CLOCK_SOURCE_INTERNAL_OSCILLATOR);

	// Set up our microsecond timer, which we'll need to handle clock bringup,
	// as several of our clocks require us to wait a controlled time.
	set_up_platform_timers();

	// Mark our early-init as complete.
	platform_early_init_complete = true;
}


/**
 * Initialize all of the system's clocks -- called by the crt0 as part of the platform setup.
 */
void platform_initialize_clocks(void)
{
	// Soft start the CPU clock.
	platform_soft_start_cpu_clock();

	// For now, enable all branch clocks. This also inherently configures the hardware necessary
	// to generate the relevant clock. TODO: disable branch clocks here, instead, and let the downstream
	// library users
	for (unsigned i = 0; i < ARRAY_SIZE(all_branch_clocks); ++i) {
		platform_enable_branch_clock(all_branch_clocks[i], false);
	}

	pr_info("System clock bringup complete.\n");
}
