/**
 * LPC43xx clock generation/control driver

 * This file is part of libgreat
 */

#include <debug.h>

#include <toolchain.h>
#include <drivers/scu.h>

#ifndef __LIBGREAT_LPC43XX_SGPIO_H__
#define __LIBGREAT_LPC43XX_SGPIO_H__

/**
 * Simple constants that convert slice letters to their numeric indices.
 */
enum {
	SGPIO_SLICE_A =  0,
	SGPIO_SLICE_B =  1,
	SGPIO_SLICE_C =  2,
	SGPIO_SLICE_D =  3,
	SGPIO_SLICE_E =  4,
	SGPIO_SLICE_F =  5,
	SGPIO_SLICE_G =  6,
	SGPIO_SLICE_H =  7,
	SGPIO_SLICE_I =  8,
	SGPIO_SLICE_J =  9,
	SGPIO_SLICE_K = 10,
	SGPIO_SLICE_L = 11,
	SGPIO_SLICE_M = 12,
	SGPIO_SLICE_N = 13,
	SGPIO_SLICE_O = 14,
	SGPIO_SLICE_P = 15
};


/**
 * Generic constants.
 */
enum {
	SGPIO_NUM_PINS                  = 16,
	SGPIO_NUM_SLICES                = 16,
	SGPIO_BITS_PER_SLICE            = 32,
	SGPIO_MAXIMUM_SLICE_CHAIN_DEPTH = 8,
};


/**
 * Constants representing drive modes for the SGPIO peripheral.
 */
enum {
	// One-bit modes.
	SGPIO_OUTPUT_MODE_1BIT      = 0x0,

	// Two-bit modes.
	SGPIO_OUTPUT_MODE_2BIT_A    = 0x1,
	SGPIO_OUTPUT_MODE_2BIT_B    = 0x2,
	SGPIO_OUTPUT_MODE_2BIT_C    = 0x3,

	// Use all SGPIO pins directly via the GPIO registers.
	SGPIO_OUTPUT_MODE_GPIO      = 0x4,

	// Four-bit modes.
	SGPIO_OUTPUT_MODE_4BIT_A    = 0x5,
	SGPIO_OUTPUT_MODE_4BIT_B    = 0x6,
	SGPIO_OUTPUT_MODE_4BIT_C    = 0x7,

	// Direct clock output (pattern generation) mode.
	SGPIO_OUTPUT_MODE_CLOCK_OUT = 0x8,

	// Eight-bit modes.
	SGPIO_OUTPUT_MODE_8BIT_A    = 0x9,
	SGPIO_OUTPUT_MODE_8BIT_B    = 0xA,
	SGPIO_OUTPUT_MODE_8BIT_C    = 0xB
};

/**
 * Parallel shift modes -- allow our system to capture or output more than one SGPIO pin at a time.
 */
enum {
	SGPIO_PARALLEL_MODE_SERIAL = 0,
	SGPIO_PARALLEL_MODE_2BIT   = 1,
	SGPIO_PARALLEL_MODE_4BIT   = 2,
	SGPIO_PARALLEL_MODE_8BIT   = 3,
};


/**
 * Constants representing each of the potential output modes for the SGPIO peripheral.
 */
enum {
	SGPIO_USE_PIN_DIRECTION_REGISTER   = 0x0,

	// Constants for each possible bus width for bidirectional direction control.
	SGPIO_DIRECTION_MODE_1BIT   = 0x4,
	SGPIO_DIRECTION_MODE_2BIT   = 0x5,
	SGPIO_DIRECTION_MODE_4BIT   = 0x6,
	SGPIO_DIRECTION_MODE_8BIT   = 0x7,
};

/**
 * Constants that specify the length of any input containations.
 */
enum {
	SGPIO_LOOP_1_SLICE  = 0,
	SGPIO_LOOP_2_SLICES = 1,
	SGPIO_LOOP_4_SLICES = 1,
	SGPIO_LOOP_8_SLICES = 1,
};


/**
 * SGPIO function overrides.
 */
enum {

	// This function prevents the given function from ever using an interrupt, even
	// if doing so would normally break its function. This override is useful when
	// the calling function plans to e.g. poll the SGPIO exchange status from an M0
	// loadable, a la Rhododendron.
	SGPIO_FUNCTION_OVERRIDE_NEVER_USE_ISR = (1 << 0)
};


/**
 * Register cluster used to control a given SGPIO interrupt.
 */
typedef struct ATTR_WORD_ALIGNED {

	// Disables the interrupt for the slices that correspond to each set bit.
	uint32_t clear;

	// Enables the interrupt for the slices that correspond to each set bit.
	uint32_t set;

	// Directly sets which slices can generate a given interrupt.
	uint32_t enable;

	// Reports whether the interrupt has fired for each slice.
	uint32_t status;

	// Clears bits in the interrupt status, marking the interrupt as handled.
	uint32_t clear_status;

	// Sets bits in the interrupt status, marking the given interrupt as pending.
	uint32_t set_status;

	// Add in a reserved word of padding to match the real device data-structure.
	RESERVED_WORDS(2);

} sgpio_interrupt_register_t;


/**
 * Output layout configuration.
 */
typedef struct ATTR_WORD_ALIGNED {
	uint32_t output_bus_mode      : 4;
	uint32_t pin_direction_source : 3;
	uint32_t                      : 25;
} sgpio_output_config_register_t;


/**
 * Shift configuration register layout.
 */
typedef struct ATTR_WORD_ALIGNED {

	// True if this slice should use an external shift clock.
	uint32_t use_external_clock    :  1;

	// For externally-clocked systems, selects the SGPIO pin that brings
	// in the external clock.
	uint32_t clock_source_pin      :  2;

	// For internally-clocked systems, selects the SGPIO slice that generates
	// our shift clock.
	uint32_t clock_source_slice    :  2;

	// Specifies whether shifts are to be gated by a logic signal on another pin,
	// or coming from another slice.
	uint32_t shift_qualifier_mode  :  2;
	uint32_t shift_qualifier_pin   :  2;
	uint32_t shift_qualifier_slice :  2;

	// If true, data will be allowed to flow between SGPIO slices, allowing chaining
	// slices together to create longer buffers, or to feed their outputs into their inputs.
	uint32_t enable_concatenation  :  1;
	uint32_t concatenation_order   :  2;

	uint32_t                       : 18;


} sgpio_shift_config_register_t;


/**
 * Specifies the behavior of our pattern-match interrupts.
 */
enum {

	// Trigger an interrupt when we first encounter a match, but do not repeatedly trigger on sustained matches.
	SGPIO_INTERRUPT_WHEN_MATCH_FOUND = 0,

	// Trigger an interrupt when we first lose a match, but do not repeatedly trigger on sustained non-matches.
	SGPIO_INTERRUPT_WHEN_MATCH_LOST = 1,

	// Trigger an interrupt for every shift that results in a non-matching pattern; which will result in interrupts
	// that recur until we match again.
	SGPIO_INTERRUPT_WHEN_NOT_MATCHING = 2,

	// Trigger an interrupt for every shift that results in a matching pattern; which will result in interrupts
	// that recur until the data no longer matches our pattern.
	SGPIO_INTERRUPT_WHEN_MATCHING = 3,
};


/**
 * Shift configuration register layout.
 */
typedef struct ATTR_WORD_ALIGNED {

	// If set, this slice will be used to drive our pattern-match triggering system,
	// rather than as a data capture slice.
	uint32_t use_as_match_trigger   : 1;

	// Determines whether we should shift on the rising edge or falling edge.
	uint32_t shift_on_falling_edge  : 1;

	// True iff we should use a clock from outside the slice. Disables local shift clock generation.
	uint32_t use_nonlocal_clock     : 1;

	// True if we should invert the locally-generated clock before sending it to be output.
	// Only has semantic meaning when we're generating -and- outputting our local clock.
	uint32_t invert_output_clock    : 1;

	// Determine when match interupts are generated -- see the SGPIO_INTERRUPT_WHEN_* constants.
	uint32_t match_interrupt_mode   : 2;

	// Determine how many bits should be captured/output'd per shift. Should be one of the SGPIO_PARALLEL_MODE_* constants.
	uint32_t parallel_mode          : 2;

	// If set, the shift clock will be gated so data is only shifted when the shift qualifier is _low_, rather than high.
	uint32_t invert_shift_qualifier : 1;

	uint32_t                        : 23;

} sgpio_feature_control_register_t;


/**
 * Register layout for the shift position register, which describes how many data shifts it takes
 * before the system automatically shifts the shadow and data registers.
 */
typedef struct ATTR_WORD_ALIGNED {

	// The number of shifts remaining until we swap the data and shadow registers.
	uint32_t shifts_remaining       :  8;

	// The total number of shifts between data/shadow buffer swaps.
	uint32_t shifts_per_buffer_swap :  8;

	uint32_t                        : 16;

} sgpio_shift_position_register_t;


/**
 * Register layout for LPC43xx timers.
 */
typedef volatile struct ATTR_WORD_ALIGNED {

	sgpio_output_config_register_t output_configuration[16];

	// SGPIO Multiplexer Register (SGPIO_MUX_CFG); controls how data shifts through each SGPIO slice.
	sgpio_shift_config_register_t shift_configuration[16];

	// Slice Mux Configuration Register (SLICE_MUX_CFG); controls the ancillary functions on each slice.
	sgpio_feature_control_register_t feature_control[16];

	// Double buffer slice data -- the active data registers and their shadow.
	// The active data register is the register that's directly updated by the SGPIO hardware;
	// the shadow register stores a second set of data that's not actively being used for shifting.
	// When a transfer is complete, the registers are swapped; allowing us to read a stable copy of the data
	// without blocking transmission.
	uint32_t data[16];
	uint32_t data_shadow[16];

	// Counter registers -- used for local clock generation.
	uint32_t sgpio_cycles_per_shift_clock[16];
	uint32_t cycle_count[16];

	// Controls when the shadow and data registers are swapped.
	sgpio_shift_position_register_t data_buffer_swap_control[16];

	// Pattern match registers -- allow us to trigger an interrupt on
	uint32_t pattern_match_a;
	uint32_t pattern_match_h;
	uint32_t pattern_match_i;
	uint32_t pattern_match_p;

	// SGPIO pin logic state, input (GPIO_INREG) -- allows the state of the SGPIO pins to be read directly.
	uint32_t sgpio_pin_state;

	// SGPIO simple output and direction register (GPIO_OUT/GPIO_OE) -- allows the SGPIO pin to be
    // driven directly to a given logic value.
	uint32_t sgpio_pin_out;
	uint32_t sgpio_pin_direction;

	// Slice count enable register (CTRL_ENABLE) -- controls whether the shift clock is allowed to run for a given slice.
	uint32_t shift_clock_enable;

	// Slice count disable register (CTRL_DISABLE) -- controls whether the SGPIO register tracks how far into each slice
	// it currently is.
	uint32_t stop_on_next_buffer_swap;

	RESERVED_WORDS(823);

	// Interrupt control for each of the slice interrupts.
	sgpio_interrupt_register_t shift_clock_interrupt;
	sgpio_interrupt_register_t exchange_clock_interrupt;
	sgpio_interrupt_register_t pattern_match_interrupt;
	sgpio_interrupt_register_t input_bit_match_interrupt;

} platform_sgpio_registers_t;

ASSERT_OFFSET(platform_sgpio_registers_t, shift_configuration,           0x040);
ASSERT_OFFSET(platform_sgpio_registers_t, feature_control,               0x080);
ASSERT_OFFSET(platform_sgpio_registers_t, data,                          0x0c0);
ASSERT_OFFSET(platform_sgpio_registers_t, data_shadow,                   0x100);
ASSERT_OFFSET(platform_sgpio_registers_t, sgpio_cycles_per_shift_clock,  0x140);
ASSERT_OFFSET(platform_sgpio_registers_t, cycle_count,                   0x180);
ASSERT_OFFSET(platform_sgpio_registers_t, data_buffer_swap_control,      0x1c0);
ASSERT_OFFSET(platform_sgpio_registers_t, pattern_match_a,               0x200);
ASSERT_OFFSET(platform_sgpio_registers_t, stop_on_next_buffer_swap,      0x220);
ASSERT_OFFSET(platform_sgpio_registers_t, shift_clock_interrupt,         0xF00);
ASSERT_OFFSET(platform_sgpio_registers_t, exchange_clock_interrupt,      0xF20);
ASSERT_OFFSET(platform_sgpio_registers_t, input_bit_match_interrupt,     0xF60);


/**
 * Configuration value that selects where the given function
 * will derive it's clock from.
 */
typedef enum {

	// Masks that allow us to separate out the individual clock source from the type of source.
	SGPIO_CLOCK_SOURCE_TYPE_MASK   = 0xF0,
	SGPIO_CLOCK_SOURCE_SELECT_MASK = 0x0F,

	// Clock sourced from another slice.
	SGPIO_CLOCK_SOURCE_TYPE_SLICE  = 0x00,
	SGPIO_CLOCK_SOURCE_SLICE_D     = 0x00,
	SGPIO_CLOCK_SOURCE_SLICE_H     = 0x01,
	SGPIO_CLOCK_SOURCE_SLICE_O     = 0x02,
	SGPIO_CLOCK_SOURCE_SLICE_P     = 0x03,

	// Clock sourced from an SGPIO pin directly.
	SGPIO_CLOCK_SOURCE_TYPE_PIN    = 0x10,
	SGPIO_CLOCK_SOURCE_SGPIO08     = 0x10,
	SGPIO_CLOCK_SOURCE_SGPIO09     = 0x11,
	SGPIO_CLOCK_SOURCE_SGPIO10     = 0x12,
	SGPIO_CLOCK_SOURCE_SGPIO11     = 0x13,

	// Clock generated within the slice itself.
	SGPIO_CLOCK_SOURCE_TYPE_LOCAL  = 0x20,
	SGPIO_CLOCK_SOURCE_COUNTER     = 0x20


} sgpio_clock_source_t;




/**
 * Configuration values that gates the shift clock, effecting providing a condition that must be met for
 * a SGPIO shift to occur.
 */
typedef enum {

	SGPIO_QUALIFIER_TYPE_SHIFT        = 4,
	SGPIO_QUALIFIER_TYPE_MASK         = 0xF0,
	SGPIO_QUALIFIER_SELECT_MASK       = 0xFF,

	// Simple shift modes: always shift on shift-clock, or never do.
	SGPIO_ALWAYS_SHIFT_ON_SHIFT_CLOCK = 0x00,
	SGPIO_NEVER_SHIFT_ON_SHIFT_CLOCK  = 0x10,

	// Slice-based qualifier modes: shift on a shift clock iff the output of a the given slice is a logic '1'.
	// The names here indicate a preference order; usually the first slice is used as the shift clock.
	// When this is impossible -- namely if the selected slice is the active slice -- the second slice in the name
	//is used.
	SGPIO_QUALIFIER_TYPE_SLICE        = 0x20,
	SGPIO_QUALIFIER_SLICE_A_OR_D      = 0x20,
	SGPIO_QUALIFIER_SLICE_H_OR_O      = 0x21,
	SGPIO_QUALIFIER_SLICE_I_OR_D      = 0x22,
	SGPIO_QUALIFIER_SLICE_P_OR_O      = 0x23,

	// Pin-based qualifier modes: shift on a shift clock iff the given SGPIO pin is reading a logic '1'.
	SGPIO_QUALIFIER_TYPE_PIN          = 0x30,
	SGPIO_QUALIFIER_SGPIO8            = 0x30,
	SGPIO_QUALIFIER_SGPIO9            = 0x31,
	SGPIO_QUALIFIER_SGPIO10           = 0x32,
	SGPIO_QUALIFIER_SGPIO11           = 0x33

} sgpio_clock_qualifier_t;


/**
 * Configuration value that selects the overall behavior of the given SGPIO function.
 */
typedef enum {

	// Mode for capturing data rapidly over a set of pins. Uses from 1-8 slices.
	SGPIO_MODE_STREAM_DATA_IN,

	// Mode for streaming data out rapidly over a set of pins. Uses from 1-8 slices.
	SGPIO_MODE_STREAM_DATA_OUT,

	// Mode for streaming -fixed- data out rapidly over a set of pins. Using this mode is very
	// similar to using STREAM_DATA_OUT, but assumes that the data buffer does not change during
	// the stream-out. This is especially useful for small buffers -- as buffers of less than "order 8"
	// (256 bytes) can often be shifted out without CPU intervention.
	SGPIO_MODE_FIXED_DATA_OUT,

	// Mode for streaming data over a set of pins that are sometimes input and sometimes output.
	// TODO: implement this
	SGPIO_MODE_STREAM_BIDIRECTIONAL,

	// Uses a single SGPIO slice to generate a clock on a single SGPIO pin.
	// Note that this is _not_ the mode to use if you want to output the clock from an existing slice -- this
	// is for generating an entirely new clock.
	SGPIO_MODE_CLOCK_GENERATION,

	// TODO: other functions?

} sgpio_function_mode_t;


/**
 * Structure that represents the clock timing for capturing data -- determines whether
 * data is captured on the rising or falling edge of the shift clock.
 */
typedef enum {
	SGPIO_CLOCK_EDGE_RISING = 0,
	SGPIO_CLOCK_EDGE_FALLING = 1
} sgpio_capture_edge_t;


/**
 * Structure that stores configuration for a given SGPIO pin.
 */
typedef struct {

	// The pin number for the given SGPIO pin.
	uint8_t sgpio_pin;

	// The LPC SCU group and pin that will be routed to the given SGPIO pin.
	uint8_t scu_group;
	uint8_t scu_pin;

	// Any pull-up or pull-down resistors to apply.
	scu_resistor_configuration_t pull_resistors;

} sgpio_pin_configuration_t;


/**
 * Structure that represents an individual piece of SGPIO functionality.
 */
typedef struct {

	// True if the given function is usable.
	bool enabled;

	// The primary behavior of the SGPIO function.
	sgpio_function_mode_t mode;

	// Pins used by the given configuration. These must compose a full bus; there must be `bus_width` entries.
	// Pins must be contiguous and in ascending order; and the first pin number must be divisible by the bus width.
	// Supports up to 8 pins per bus. (If you want a 16-pin bus, you'll need to set up two 8-pin functions.)
	sgpio_pin_configuration_t *pin_configurations;
	uint8_t bus_width;

	// The source for the SGPIO shift clock for data modes. Ignored in clock generation mode.
	sgpio_clock_source_t shift_clock_source;
	sgpio_capture_edge_t shift_clock_edge;

	// If our shift clock source is one of the SGPIO pins, we need to specify how it should be configured
	// in the SCU.
	sgpio_pin_configuration_t *shift_clock_input;

	// If this function is generating a clock, either for data timing or for clock generation mode,
	// this value selects the clock's frequency, in Hz. Not used if an external clock or another slice's clock is used.
	uint32_t shift_clock_frequency;

	// Determines the conditions under which a shift clock edge will cause data to shift in or out.
	// Used for the data stream modes. See `sgpio_clock_qualifier_t` for more information.
	sgpio_clock_qualifier_t shift_clock_qualifier;
	bool shift_clock_qualifier_is_active_low;

	// If we have a pin qualifier for SGPIO shifting, specify how to set that up in the SCU.
	sgpio_pin_configuration_t *shift_clock_qualifier_input;

	// If desired, we can output any _generated_ shift clock on one of the unused SGPIO pins.
	// This specifies the pin on which the shift clock should be output. Provide NULL if output is not desired.
	// This value is meaningless if .shift_clock_source is not a local counter.
	sgpio_pin_configuration_t *shift_clock_output;

	// Circular buffer that will contain the -packed- binary data to be scanned in or out.
	// Must be sized to a power of two -- and the size is stored as an order -- that is, as the log2() of the size.
	// Bidirectional modes scan the data in the buffer out, and replace it with data scanned in.
	void *buffer;
	uint8_t buffer_order;

	// Bidirectional mode only:
	// Circular buffer that contains direction information. For parallel busses, direction is always a packed
	// set of two-bit values, whose lsb controls the direction of bit zero, and the msb control the direction of
	// all other bits. For single-bit functions, this is a packed set of one-bit values that determine the direction
	// that bit during the current cycle.
	void *direction_buffer;
	uint8_t direction_buffer_order;

	// Current position in our buffers.
	uint32_t position_in_buffer;
	uint32_t position_in_direction_buffer;

	// If this variable is set to a (small) non-zero value, the count will stop after a given number of shift clock
	// cycles. This _must_ be shorter than the buffer depth in samples, to be achievable. Typically 32 or fewer is
	// safe for a single 8-bit data operation, with the 'safe' size doubling as you halve the bus width.
	// TODO: allow for larger shift count handling in our interrupt template body?
	uint32_t shift_count_limit;

	// Fill count -- counts the number of times the SGPIO driver has placed data into the driver.
	// The user can decrement this when consuming data from the buffer to keep a count of "data available".
	uint32_t data_in_buffer;

	// Special use-case only override flags.
	// These are nearly always unused.
	uint32_t overrides;

	//
	// Set automatically by the driver -- not for user use.
	//

	// The slice that serves as our "I/O boundary".
	uint8_t io_slice;
	uint8_t buffer_depth_order;

	// The slice that contains the direction data currently being used.
	uint8_t direction_slice;
	uint8_t direction_buffer_depth_order;

} sgpio_function_t;


/**
 * Structure storing the state of a libgreat SGPIO object.
 */
typedef struct {

	bool running;

	// Descriptions of each unique SGPIO behavior, which can be things
	// such as e.g. a logic analyzer scan-in or a pattern generator scan-out.
	sgpio_function_t *functions;
	size_t function_count;

	//
	// Automatically generated -- you don't need to provide these.
	//

	// Mask that contains an indication of which slices are currently being used.
	uint32_t slices_in_use;

	// Mask that contains an indication of which pins are currently being used.
	uint32_t pins_in_use;

	// Mask that indicates which slices have an associated swap IRQ.
	uint32_t swap_irqs_required;

	// Reference to the SGPIO controller's registers.
	platform_sgpio_registers_t *reg;

} sgpio_t;

/**
 * Sets up an SGPIO instance to run a provided set of functions.
 *
 * @param sgpio An SGPIO instance object with its functions array already pre-defined. This array specifies how the
 *     SGPIO hardware will be used to interface with the SGPIO pins.
 * @return 0 on success, or an error code on failure.
 */
int sgpio_set_up_functions(sgpio_t *sgpio);

/**
 * Prints a verbose dump of SGPIO peripheral's state.
 */
void sgpio_dump_configuration(loglevel_t loglevel, sgpio_t *sgpio, bool include_unused);


/**
 * Starts the run of the relevant object's SGPIO functions.
 */
void sgpio_run(sgpio_t *sgpio);


/**
 * Halts the execution of the relevant object's SGPIO functions.
 */
void sgpio_halt(sgpio_t *sgpio);


/**
 * @return true iff any SGPIO functionality is currently running
 */
bool sgpio_running(sgpio_t *sgpio);


/**
 * @returns a reference to the register bank for the device's SGPIO
 */
platform_sgpio_registers_t *platform_get_sgpio_registers(void);


/**
 * @returns The clock source constant for using the given pin as a clock source.
 */
inline sgpio_clock_source_t sgpio_clock_source_from_pin_configuration(sgpio_pin_configuration_t pin)
{
	switch (pin.sgpio_pin) {
		case  8: return SGPIO_CLOCK_SOURCE_SGPIO08;
		case  9: return SGPIO_CLOCK_SOURCE_SGPIO08;
		case 10: return SGPIO_CLOCK_SOURCE_SGPIO08;
		case 11: return SGPIO_CLOCK_SOURCE_SGPIO08;
		default:
			pr_error("sgpio: error: specified a pin that could not be used as a clock source!\n");
			return -1;
	}
}


/**
 * Runs an SGPIO function, and blocks until it completes. (Halts the SGPIO function when complete.)
 * Should only be used if your SGPIO function has a fixed termination condition (e.g. a shift limit).
 */
static inline void sgpio_run_blocking(sgpio_t *sgpio)
{
	sgpio_run(sgpio);
	while(sgpio_running(sgpio));
	sgpio_halt(sgpio);
}


#endif
