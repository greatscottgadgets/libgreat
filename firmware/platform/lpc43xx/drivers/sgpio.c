/*
 * SGPIO Driver for the LPC43xx series.
 *
 * This file is part of libgreat.
 */


#include <toolchain.h>

#include <debug.h>
#include <errno.h>

#include <drivers/scu.h>
#include <drivers/sgpio.h>
#include <drivers/platform_clock.h>
#include <drivers/arm_vectors.h>

//#define pr_debug pr_info

// FIXME: handle branch clock frequency changes by providing a _handle_clock_frequency_change callback!

void sgpio_dump_slice_configuration(loglevel_t loglevel, sgpio_t *sgpio, uint8_t slice);

// Non-public 'imports' -- functions from sgpio_isr.c that aren't for public consumption.
interrupt_service_routine_t sgpio_generate_data_shuttle_isr(sgpio_t *sgpio);
int sgpio_generate_isr_for_function(sgpio_function_t *function);
void sgpio_handle_data_prepopulation(sgpio_t *sgpio);
bool sgpio_isr_necessary_for_function(sgpio_function_t *function);



typedef struct {

	// The SGPIO number to be configured.
	uint8_t sgpio : 4;

	// The pin and group in the SCU.
	uint8_t pin : 5;
	uint8_t group : 4;

	// The function number associated with the SGPIO + pin combination.
	uint8_t function : 3;

} sgpio_scu_function_t;


/**
 * Constant table storing each of the possible function mappings for SGPIO pins.
 */
static const sgpio_scu_function_t scu_mappings[] = {

	// In the order of the datasheet table. Spacing indicates page breaks, roughly.
	{ .sgpio = 0,  .group =  0, .pin =  0, .function = 3},
	{ .sgpio = 1,  .group =  0, .pin =  1, .function = 3},
	{ .sgpio = 7,  .group =  1, .pin =  0, .function = 6},
	{ .sgpio = 8,  .group =  1, .pin =  1, .function = 3},
	{ .sgpio = 9,  .group =  1, .pin =  2, .function = 3},
	{ .sgpio = 10, .group =  1, .pin =  3, .function = 2},
	{ .sgpio = 11, .group =  1, .pin =  4, .function = 2},
	{ .sgpio = 15, .group =  1, .pin =  5, .function = 6},
	{ .sgpio = 14, .group =  1, .pin =  6, .function = 6},
	{ .sgpio =  8, .group =  1, .pin = 12, .function = 6},
	{ .sgpio =  9, .group =  1, .pin = 13, .function = 6},
	{ .sgpio = 10, .group =  1, .pin = 14, .function = 6},
	{ .sgpio =  2, .group =  1, .pin = 15, .function = 2},
	{ .sgpio =  3, .group =  1, .pin = 16, .function = 2},
	{ .sgpio = 11, .group =  1, .pin = 17, .function = 6},
	{ .sgpio = 12, .group =  1, .pin = 18, .function = 6},
	{ .sgpio = 13, .group =  1, .pin = 20, .function = 6},
	{ .sgpio =  4, .group =  2, .pin =  0, .function = 1},


	{ .sgpio =  5, .group =  2, .pin =  1, .function = 0},
	{ .sgpio =  6, .group =  2, .pin =  2, .function = 0},
	{ .sgpio = 12, .group =  2, .pin =  3, .function = 0},
	{ .sgpio = 13, .group =  2, .pin =  4, .function = 0},
	{ .sgpio = 14, .group =  2, .pin =  5, .function = 0},
	{ .sgpio =  7, .group =  2, .pin =  6, .function = 0},
	{ .sgpio = 15, .group =  2, .pin =  8, .function = 0},
	{ .sgpio =  8, .group =  4, .pin =  2, .function = 7},
	{ .sgpio =  9, .group =  4, .pin =  3, .function = 7},
	{ .sgpio = 10, .group =  4, .pin =  4, .function = 7},


	{ .sgpio = 11, .group =  4, .pin =  5, .function = 7},
	{ .sgpio = 12, .group =  4, .pin =  6, .function = 7},
	{ .sgpio = 13, .group =  4, .pin =  8, .function = 7},
	{ .sgpio = 14, .group =  4, .pin =  9, .function = 7},
	{ .sgpio = 15, .group =  4, .pin = 10, .function = 7},

	{ .sgpio =  4, .group =  6, .pin =  3, .function = 2},
	{ .sgpio =  5, .group =  6, .pin =  6, .function = 2},
	{ .sgpio =  6, .group =  6, .pin =  7, .function = 2},
	{ .sgpio =  7, .group =  6, .pin =  8, .function = 2},


	{ .sgpio =  4, .group =  7, .pin =  0, .function = 7},
	{ .sgpio =  5, .group =  7, .pin =  1, .function = 7},
	{ .sgpio =  6, .group =  7, .pin =  2, .function = 7},
	{ .sgpio =  7, .group =  7, .pin =  7, .function = 7},

	{ .sgpio = 3,  .group =  9, .pin =  5, .function = 6},
	{ .sgpio = 8,  .group =  9, .pin =  6, .function = 6},

	// FIXME: continue this table for the BGA-only port groups?

};


/**
 * @returns a reference to the register bank for the device's SGPIO
 */
static platform_sgpio_registers_t *platform_get_sgpio_registers(void)
{
	return (platform_sgpio_registers_t *)0x40101000;
}


/**
 * Retrieves the SCU function number for the given LPC pin and SGPIO pin number.
 *
 * @return The function number, or 0xFF if no mapping could be found.
 */
static uint8_t sgpio_get_scu_function_for_pin_config(sgpio_pin_configuration_t *config)
{
	// Search our table for a
	for (unsigned i = 0; i < ARRAY_SIZE(scu_mappings); ++i) {
		const sgpio_scu_function_t *mapping = &scu_mappings[i];

		// Search until we have a mapping from the relevant SGPIO pin number to
		// our LPC pin information.
		bool match =
			(mapping->group == config->scu_group) &&
			(mapping->pin   == config->scu_pin)   &&
			(mapping->sgpio == config->sgpio_pin);

		if (match) {
			return mapping->function;
		}
	}

	// If we didn't find a function, return an invalid value to indicate an error.
	return 0xFF;
}


/**
 * Configure the SGPIO hardware to handle the relevant configuration.
 *
 * @return 0 on success, or an error code on failure
 */
static int sgpio_set_up_pin(sgpio_t *sgpio, sgpio_pin_configuration_t *pin_config)
{
	// Look up the SCU function number for the given SGPIO -> pin mapping.
	uint8_t function = sgpio_get_scu_function_for_pin_config(pin_config);

	// If we can't figure out how to map the SGPIO pin to the relevant LPC pin, fail out.
	if (function == 0xFF) {
		pr_error("error: sgpio: couldn't figure out how to map SGPIO%u to P%u_%u\n",
			pin_config->sgpio_pin, pin_config->scu_group, pin_config->scu_pin);
		return EINVAL;
	}

	// Finally, configure the given mapping...
	platform_scu_configure_pin_fast_io(pin_config->scu_group, pin_config->scu_pin, function, pin_config->pull_resistors);

	// ... and mark the pin as used.
	sgpio->pins_in_use |= (1 << pin_config->sgpio_pin);
	return 0;
}

/**
 * @return the index of the SGPIO slice that corresponds to the given SGPIO pin
 */
int sgpio_slice_for_io(uint8_t pin)
{
	const uint8_t input_slice_mappings[] = {
		SGPIO_SLICE_A, SGPIO_SLICE_I, SGPIO_SLICE_E, SGPIO_SLICE_J,
		SGPIO_SLICE_C, SGPIO_SLICE_K, SGPIO_SLICE_F, SGPIO_SLICE_L,
		SGPIO_SLICE_B, SGPIO_SLICE_M, SGPIO_SLICE_G, SGPIO_SLICE_N,
		SGPIO_SLICE_D, SGPIO_SLICE_O, SGPIO_SLICE_H, SGPIO_SLICE_P
	};

	if (pin >= SGPIO_NUM_PINS) {
		return -EINVAL;
	}

	return input_slice_mappings[pin];
}

/**
 * @returns the SGPIO pin associated with the given slice; which is the pin
 *      used for I/O to the given slice when we're in single-bit (serial) mode
 */
int sgpio_io_pin_for_slice(uint8_t slice)
{
	// Search our look-up table for a pin that results in the given slice.
	for (int i = 0; i < SGPIO_NUM_PINS; ++i) {
		if (sgpio_slice_for_io(i) == slice) {
			return i;
		}
	}

	return -EINVAL;
}


static int sgpio_slice_for_clockgen(uint8_t pin)
{
	const uint8_t clockgen_slice_mappings[] = {
		SGPIO_SLICE_B, SGPIO_SLICE_D, SGPIO_SLICE_E, SGPIO_SLICE_H,
		SGPIO_SLICE_C, SGPIO_SLICE_F, SGPIO_SLICE_O, SGPIO_SLICE_P,
		SGPIO_SLICE_A, SGPIO_SLICE_M, SGPIO_SLICE_G, SGPIO_SLICE_N,
		SGPIO_SLICE_I, SGPIO_SLICE_J, SGPIO_SLICE_K, SGPIO_SLICE_L
	};

	return clockgen_slice_mappings[pin];
}


/**
 * @returns the SGPIO pin that's next in the concatenation for a given slice; assumes no
 *    wraparound. Returns invalid responses for impossible concatenations.
 */
static int sgpio_slice_in_concatenation(uint8_t io_slice, uint8_t depth)
{
	// Our algorithm for finding the next slice in our concatenation is relatively simple:
	// first, we find the SGPIO pin that corresponds to the active slice; and then we look
	// for the next slice. Given we're always starting from the first slice in a chain
	// (the I/O slice), we never have to worry about wrap-around, so this works.
	uint8_t pin = sgpio_io_pin_for_slice(io_slice);
	return sgpio_slice_for_io(pin + depth);
}



/**
 * Configures the shift clock for using a slice based on its function description.
 */
static int sgpio_set_up_clocking(sgpio_t *sgpio, sgpio_function_t *function, uint8_t slice)
{
	uint8_t clock_source_type = function->shift_clock_source & SGPIO_CLOCK_SOURCE_TYPE_MASK;
	uint8_t clock_source      = function->shift_clock_source & SGPIO_CLOCK_SOURCE_SELECT_MASK;

	// Set up the shift clock source for the given slice.
	sgpio->reg->shift_configuration[slice].use_external_clock = (clock_source_type == SGPIO_CLOCK_SOURCE_TYPE_PIN);
	sgpio->reg->feature_control[slice].use_nonlocal_clock     = (clock_source_type != SGPIO_CLOCK_SOURCE_TYPE_LOCAL);
	sgpio->reg->feature_control[slice].shift_on_falling_edge  = function->shift_clock_edge;

	// Set up both the slice-clock select and the pin-clock select lines. The device will only use one; but the other
	// is safely ignored, so there's no harm simplifying and setting both.
	sgpio->reg->shift_configuration[slice].clock_source_slice = clock_source;
	sgpio->reg->shift_configuration[slice].clock_source_pin   = clock_source;

	// Finally, compute the local shift clock parameters, if the shift clock is used.
	if (clock_source_type == SGPIO_CLOCK_SOURCE_TYPE_LOCAL) {
		platform_clock_control_register_block_t *ccu = get_platform_clock_control_registers();
		uint32_t sgpio_clock_frequency = platform_get_branch_clock_frequency(&ccu->periph.sgpio);

		// Compute the shift clock divider, based on the current branch clock frequency.
		// TODO: do we want to round, here?
		uint32_t clock_divider = sgpio_clock_frequency / function->shift_clock_frequency;

		// If we couldn't figure out a clock divider, bail out!
		if (clock_divider == 0) {
			pr_error("error: sgpio slice %d: could not meet timing! could not produce a %" PRIu32 " clock from a %"
			          PRIu32 " input clock.\n", function->shift_clock_frequency, sgpio_clock_frequency);
			return EINVAL;
		}

		// Set up counter that will generate the relevant clock.
		sgpio->reg->sgpio_cycles_per_shift_clock[slice] = clock_divider - 1;
		sgpio->reg->cycle_count[slice] = clock_divider - 1;

		// Update the function's knowledge of the shift clock with the actual clock rate achieved.
		// This will be different if the SGPIO clock isn't evenly divisible by the desired clock rate.
		function->shift_clock_frequency = sgpio_clock_frequency / clock_divider;
	}

	return 0;
}


/**
 * Configures the shift clock qualifier for using a slice based on its function description.
 */
static int sgpio_set_up_shift_condition(sgpio_t *sgpio, sgpio_function_t *function, uint8_t slice)
{
	uint8_t qualifier_type   = function->shift_clock_qualifier & SGPIO_QUALIFIER_TYPE_MASK;
	uint8_t qualifier_source = function->shift_clock_source    & SGPIO_QUALIFIER_SELECT_MASK;

	// Set up the qualifier type, which determines when the shift clock will trigger a data shift.
	sgpio->reg->shift_configuration[slice].shift_qualifier_mode = qualifier_type >> SGPIO_QUALIFIER_TYPE_SHIFT;

	// Set up both the shift qualifier pin and slice sources. Only register corresponding to the active mode is used;
	// but there's no harm in setting the other.
	sgpio->reg->shift_configuration[slice].shift_qualifier_pin   = qualifier_source;
	sgpio->reg->shift_configuration[slice].shift_qualifier_slice = qualifier_source;

	// Finally, set up the polarity of our qualifier.
	sgpio->reg->feature_control[slice].invert_shift_qualifier    = function->shift_clock_qualifier_is_active_low;

	return 0;
}


/**
 * Sets up the control/data swapping that allows us to double-buffer SGPIO data.
 *
 * @param slice The slice to configure.
 * @param total_concatinated_slices The total number of slices in the current concatenation.
 * @param bus_width The total number of bits taken in per shift.
 */
void sgpio_set_up_double_buffering(sgpio_t *sgpio, uint8_t slice, uint8_t total_concatenated_slices, uint8_t bus_width)
{
	uint8_t shifts_per_swap = (SGPIO_BITS_PER_SLICE * total_concatenated_slices) / bus_width;

	// Compute the final value of the swap-control register...
	sgpio_shift_position_register_t swap_control_value = {
		.shifts_per_buffer_swap = shifts_per_swap - 1,
		.shifts_remaining       = shifts_per_swap - 1,
	};

	// ... and apply it to the relevant slice.
	sgpio->reg->data_buffer_swap_control[slice] = swap_control_value;

	// Ensure double buffering is enabled for the relevant slice.
	sgpio->reg->disable_double_buffering &= ~(1 << slice);
}


/**
 * Configures the shape of the bus created through our SGPIO slices, based on its function description.
 */
static int sgpio_set_up_bus_topology(sgpio_t *sgpio, sgpio_function_t *function, uint8_t slice)
{
	// Select the PARALLEL mode based on the bus width requested.
	switch (function->bus_width) {

		// Set the SGPIO modes based on our reported bus width.
		case 1:
			sgpio->reg->feature_control[slice].parallel_mode = SGPIO_PARALLEL_MODE_SERIAL;
			break;
		case 2:
			sgpio->reg->feature_control[slice].parallel_mode = SGPIO_PARALLEL_MODE_2BIT;
			break;

		case 3:
			pr_warning("sgpio: warning: cannot create a 3-bit bus; creating a 4-bit bus instead.\n");
			function->bus_width = 4;
			// Falls through.

		case 4:
			sgpio->reg->feature_control[slice].parallel_mode = SGPIO_PARALLEL_MODE_4BIT;
			break;

		case 5:
		case 6:
		case 7:
			pr_warning("sgpio: warning: cannot create a %" PRIu8 "-bit bus; creating an 8-bit bus instead.\n",
			            function->bus_width);
			function->bus_width = 8;
			// Falls through.

		case 8:
			sgpio->reg->feature_control[slice].parallel_mode = SGPIO_PARALLEL_MODE_8BIT;
			break;

		default:
			pr_error("sgpio: error: cannot create a %" PRIu8 "-bit bus!\n", function->bus_width);
			return EINVAL;
	}

	// Set the relevant slice to draw input from its I/O pin; and use only the current slice for buffering.
	sgpio->reg->shift_configuration[slice].enable_concatenation = 0;
	function->buffer_depth_order = 0;

	// Finally, set up the shift position register for the given slice.
	// Until we expand our chain via concatenation, we use only a single 32-bit buffer, divided by the bus width.
	sgpio_set_up_double_buffering(sgpio, function->io_slice, 1, function->bus_width);

	return 0;
}


/**
 * Sets up a SGPIO instance to run a provided SGPIO function.
 * Modifies the pins and slices relevant to the provided configuration, but leaves unrelated pins/slices
 * unconfigured.
 *
 * @param sgpio An SGPIO instance object with its functions array already pre-defined. This array specifies how the
 *     SGPIO hardware will be used to interface with the SGPIO pins.
 * @return 0 on success, or an error code on failure.
 */
int sgpio_set_up_function(sgpio_t *sgpio, sgpio_function_t *function)
{
	int rc;

	uint8_t first_pin_number = function->pin_configurations[0].sgpio_pin;

	// If this function is disabled, we don't need to do any setup.
	// Return immediately.
	if (!function->enabled) {
		return 0;
	}

	// First, we'll route each used pin to the SGPIO module in the SCU, so it can be used by our downstream functions.
	for (unsigned i = 0; i < function->bus_width; ++i) {
		rc = sgpio_set_up_pin(sgpio, &function->pin_configurations[i]);
		if (rc) {
			return rc;
		}
	}

	// Next, we'll set up the slice that performs the I/O for the given function.
	// This is the core piece of hardware that acts as our I/O boundary.

	// First, set up the mode-specific attributes of the SGPIO configuration.
	// Note that we almost always deal with the first pin, as the SGPIO peripheral tends
	// to squish parallel data into the slice for the first pin.
	switch(function->mode) {

		case SGPIO_MODE_STREAM_DATA_IN:
			function->io_slice = sgpio_slice_for_io(first_pin_number);
			break;

		case SGPIO_MODE_STREAM_DATA_OUT:
		case SGPIO_MODE_FIXED_DATA_OUT:
			function->io_slice = sgpio_slice_for_io(first_pin_number);
			break;

		case SGPIO_MODE_CLOCK_GENERATION:
			function->io_slice = sgpio_slice_for_clockgen(first_pin_number);
			break;

		default:
			pr_error("sgpio: error: SGPIO mode %d not yet implemented!\n", function->mode);
			return -ENOSYS;
	}

	pr_debug("sgpio: function using IO slice %u\n", function->io_slice);

	// Set up clocking for the relevant slice.
	rc = sgpio_set_up_clocking(sgpio, function, function->io_slice);
	if (rc) {
		return rc;
	}

	// Set up the qualifier for the relevant slice.
	rc = sgpio_set_up_shift_condition(sgpio, function, function->io_slice);
	if (rc) {
		return rc;
	}

	// Set up the data width for the relevant slice.
	rc = sgpio_set_up_bus_topology(sgpio, function, function->io_slice);
	if (rc) {
		return rc;
	}

	// Finally, mark the slice as used.
	sgpio->slices_in_use |= (1 << function->io_slice);
	pr_debug("sgpio: IO slice mask is now 0x%04x\n", sgpio->slices_in_use);

	return 0;
}



bool sgpio_slices_for_buffer_free(sgpio_t *sgpio, uint8_t io_slice, uint8_t first_new_slice_depth, uint8_t buffer_depth_slices)
{
	// Check each of the slices for the proposed buffer, and verify that they're free.
	for (unsigned i = first_new_slice_depth; i < buffer_depth_slices; ++i) {

		// Grab the slice that corresponds to index "i" in our theoretical buffer.
		// We effectively look up the next used slice in the reference manual Table 277.
		uint8_t target_slice = sgpio_slice_in_concatenation(io_slice, i);
		uint16_t slice_mask  = 1 << target_slice;

		pr_debug("sgpio: checking to see if slice %u is free\n", target_slice);

		// Check to see if the relevant slice is currently in use; and fail out if it is.
		if (sgpio->slices_in_use & slice_mask) {
			pr_debug("sgpio: not doubling; slice %u is in use\n", target_slice);
			return false;
		}
	}

	return true;
}

void sgpio_copy_slice_properties(sgpio_t *sgpio, uint8_t to_slice, uint8_t from_slice)
{
	// Copy the core contents of our slice control registers.
	sgpio->reg->shift_configuration[to_slice] = sgpio->reg->shift_configuration[from_slice];
	sgpio->reg->feature_control[to_slice]     = sgpio->reg->feature_control[from_slice];

	// Copy the clock generation / timing properties.
	sgpio->reg->sgpio_cycles_per_shift_clock[to_slice] = sgpio->reg->sgpio_cycles_per_shift_clock[from_slice];
	sgpio->reg->cycle_count[to_slice] = sgpio->reg->cycle_count[from_slice];

	// Determine how often we swap the data and shadow buffers.
	sgpio->reg->data_buffer_swap_control[to_slice] = sgpio->reg->data_buffer_swap_control[from_slice];

	// TODO: handle "pos disable" fields for clock generation
}


/**
 * @returns the buffer depth necessary to contain a user buffer, or the provided maximum slice depth, whichever is lower.
 *    Used to ensure a concatenation depth doesn't allocate more than we can transfer to/from the user buffer.
 */
static uint8_t sgpio_limit_buffer_depth_to_user_buffer_size(sgpio_function_t *function, uint8_t maximum_depth)
{
	// Compute the user buffer size in slices...
	uint32_t buffer_size_bytes = (1 << function->buffer_order);
	uint32_t buffer_size_slices = buffer_size_bytes / sizeof(uint32_t);

	// Special case: if we have less than a slice worth of data, return a maximum order of a single slice.
	if (buffer_size_bytes < sizeof(uint32_t)) {
		return 1;
	}

	// Special case: if this is fixed-data-out mode, we can use both the slice and the shadow registers
	// to store our output data, so we can actually halve the necessary buffer size in slices.
	if ((function->mode == SGPIO_MODE_FIXED_DATA_OUT) && (buffer_size_slices > 1)) {
		buffer_size_slices /= 2;
	}

	// Return either the user buffer size in slices, or the provided depth limit, whichever is lower.
	return (buffer_size_slices > maximum_depth) ? maximum_depth : buffer_size_slices;
}


/**
 *  @returns The maximum buffer depth for the given function; as not all functions can utilize a max-size buffer.
 */
static uint8_t sgpio_maximum_useful_buffer_depth_for_function(sgpio_function_t *function)
{
	switch (function->mode) {

		// Clock generation mode only ever uses a single slice, so there's no point in doubling.
		case SGPIO_MODE_CLOCK_GENERATION:
			return 1;

		// In unidirectional modes, it makes sense to use as much buffer as we can -- so either
		// the maximum chain length, or the amount of slices necessary to contain a full user buffer,
		// whichever is smaller.
	    case SGPIO_MODE_STREAM_DATA_IN:
		case SGPIO_MODE_STREAM_DATA_OUT:
		case SGPIO_MODE_FIXED_DATA_OUT:
			return sgpio_limit_buffer_depth_to_user_buffer_size(function, SGPIO_MAXIMUM_SLICE_CHAIN_DEPTH);

		// Bidirectional modes are a little more complex. If our I/O slice is 'A', we can use a full-depth buffer;
		// otherwise we're stuck using only half the maximum size. This is because we need to reserve some slices from
		// D/H/O/P to set the output's direction.
		case SGPIO_MODE_STREAM_BIDIRECTIONAL: {
			uint32_t maximum_bidirectional_depth = SGPIO_MAXIMUM_SLICE_CHAIN_DEPTH;

			// If we're not working with a slice-chain starting with A, have the maximum slice depth.
			if (function->io_slice != SGPIO_SLICE_A) {
				maximum_bidirectional_depth /= 2;
			}

			return sgpio_limit_buffer_depth_to_user_buffer_size(function, maximum_bidirectional_depth);
		}

	}
}


bool sgpio_attempt_to_double_buffer_size(sgpio_t *sgpio, sgpio_function_t *function)
{
	// Figure out the current configuration order.
	uint8_t concat_order = function->buffer_depth_order;
	uint8_t desired_concatenation_order = concat_order + 1;

	// Convert the depth-order to a number of relevant slices.
	uint8_t buffer_depth_slices  = 1 << concat_order;
	uint8_t desired_buffer_depth = 1 << desired_concatenation_order;
	pr_debug("sgpio: attempting to double buffer from %u to %u slices\n", buffer_depth_slices, desired_buffer_depth);

	// If we've already maximized this slice's buffer depth, we can't optimize any further.
	if (desired_buffer_depth > sgpio_maximum_useful_buffer_depth_for_function(function)) {
		pr_debug("sgpio: cannot optimize further; already max size!\n");
		return false;
	}

	// If any of the necessary slices are in use, we can't optimize any further. Abort.
	if (!sgpio_slices_for_buffer_free(sgpio, function->io_slice, buffer_depth_slices, desired_buffer_depth)) {
		pr_debug("sgpio: cannot optimize further -- not enough slices free!\n");
		return false;
	}

	pr_debug("sgpio: doubling buffer!\n");
	function->buffer_depth_order = desired_concatenation_order;

	// Update our double-buffering to now take into account the entire chain of concatenated slices.
	sgpio_set_up_double_buffering(sgpio, function->io_slice, desired_buffer_depth, function->bus_width);

	// If all of our conditions here are met, we can grab the relevant slices.
	// Iterate over each of the slices in our new buffer, and configure them.
	for (unsigned i = 0; i < desired_buffer_depth; ++i) {
		uint8_t target_slice = sgpio_slice_in_concatenation(function->io_slice, i);
		volatile sgpio_shift_config_register_t *shift_config = &sgpio->reg->shift_configuration[target_slice];

		// If this element isn't the I/O slice, copy the I/O slice's properties to it.
		if (target_slice != function->io_slice) {
			sgpio_copy_slice_properties(sgpio, target_slice, function->io_slice);
		}

		// If this is the I/O slices, and we're in pure-input mode, accept input from the I/O pin.
		// Otherwise, always accept input from a concatinated slice. This builds maximum length chains;
		// and creates one big self-loop. For most modes, the self-loop is inconsequential; but for fixed-pattern
		// modes this is necessary.
		shift_config->enable_concatenation =
			(function->mode != SGPIO_MODE_STREAM_DATA_IN) || (target_slice != function->io_slice);

		// TODO: does this also work for bidirectional mode?

		// Set the slice's new concatenation order.
		shift_config->concatenation_order  = desired_concatenation_order;

		// Mark the new slice as used.
		sgpio->slices_in_use |= (1 << target_slice);
	}

	return true;
}


/**
 * Attempt to perform an SGPIO buffer optimization. We can concatenate multiple SGPIO slices
 * to construct larger SGPIO buffers; this method attempts to coalesce any unused slices into
 * our existing buffers to reduce how often we have to empty the SGPIO shadow buffer.
 *
 * @returns True if the current state is believed to be optimal; or false if this method
 * 		should be called again.
 */
bool sgpio_attempt_buffer_optimization(sgpio_t *sgpio)
{
	// Start off by assuming our configuration is optimal, until one of our optimizations
	// provides otherwise.
	bool already_optimal = true;

	// Iterate over each of our SGPIO functions, and attempt to optimize it.
	for (unsigned i = 0; i < sgpio->function_count; ++i) {
		sgpio_function_t *function = &sgpio->functions[i];
		bool optimization_achieved;

		switch(function->mode) {
			case SGPIO_MODE_STREAM_DATA_IN:
			case SGPIO_MODE_STREAM_DATA_OUT:
			case SGPIO_MODE_FIXED_DATA_OUT:
				optimization_achieved = sgpio_attempt_to_double_buffer_size(sgpio, function);
				break;

			// No optimization is possible for clock generators.
			case SGPIO_MODE_CLOCK_GENERATION:
				optimization_achieved = false;
				break;

			default:
				pr_error("sgpio: error: SGPIO mode %d not yet implemented!\n", function->mode);
				return -ENOSYS;
		}

		// We'll continue trying to apply optimizations until we're sure we can't improve -- e.g. until all
		// optimization passes fail. If we managed an optimization, we're not at that point yet, so mark our
		// optimization as incomplete.
		if (optimization_achieved) {
			pr_debug("sgpio: likely not yet optimal, continuing\n");
			already_optimal = false;
		}
	}

	return already_optimal;
}


/**
 * @returns the SGPIO output mode for bidirectional function of the relevant width
 */
static uint32_t sgpio_output_mode_for_output_buffer(uint8_t bus_width)
{
	// We use simple Mode A for all pure-output buffers, as that allows us to optimize for buffer depth.
	// Mode C is tempting, and would be nice, as it would keep the register population order the same as in
	// pure-input -- but it's optimized for avoiding slices D/O/H/P for bidirectional, and thus prevents us
	// from achieving a full output buffer depth on pins 8-15.
	switch (bus_width) {
		case 1:
			return SGPIO_OUTPUT_MODE_1BIT;
		case 2:
			return SGPIO_OUTPUT_MODE_2BIT_A;
		case 3:
		case 4:
			return SGPIO_OUTPUT_MODE_4BIT_A;
		case 5:
		case 6:
		case 7:
		case 8:
			return SGPIO_OUTPUT_MODE_8BIT_A;
	}

	// In any invalid case, log an error, and then switch to GPIO mode.
	pr_warning("sgpio: invalid bus width detected!\n");
	return SGPIO_OUTPUT_MODE_GPIO;
}


/**
 * Sets up the SGPIO pins used by the given function to provide any relevant output functionality.
 */
static void sgpio_set_up_output_pins_for_function(sgpio_t *sgpio, sgpio_function_t *function)
{
	// Iterate over each pin relevant to the given function...
	for (unsigned i = 0; i < function->bus_width; ++i) {
		uint8_t pin_number = function->pin_configurations[i].sgpio_pin;
		volatile sgpio_output_config_register_t *output_config = &sgpio->reg->output_configuration[pin_number];

		// ... and set it up to match the configured mode.
		switch (function->mode) {

			// For input mode, make sure the relevant pin isn't ever being driven.
			// Our short method for accomplishign this is to switch direction-selection to the GPIO direction mode,
			// which uses the sgpio_pin_direction register to select the given pin's direction, and then select Input.
			case SGPIO_MODE_STREAM_DATA_IN:
				output_config->pin_direction_source = SGPIO_USE_PIN_DIRECTION_REGISTER;
				sgpio->reg->sgpio_pin_direction     = ~(1 << pin_number);
				break;


			// Handle our unidirectional data outputs.
			case SGPIO_MODE_STREAM_DATA_OUT:
			case SGPIO_MODE_FIXED_DATA_OUT:

				// Set the output mode to use our I/O slice as the output boundary...
				output_config->output_bus_mode      = sgpio_output_mode_for_output_buffer(function->bus_width);

				// ... and set the pin to always be a simple output.
				output_config->pin_direction_source = SGPIO_USE_PIN_DIRECTION_REGISTER;
				sgpio->reg->sgpio_pin_direction    |= 1 << pin_number;
				break;


			// Handle our slice clock generation modes.
			case SGPIO_MODE_CLOCK_GENERATION:

				// Set the relevant pin to output the I/O slice's clock.
				output_config->output_bus_mode      = SGPIO_OUTPUT_MODE_CLOCK_OUT;

				// ... and set the pin to always be a simple output.
				output_config->pin_direction_source = SGPIO_USE_PIN_DIRECTION_REGISTER;
				sgpio->reg->sgpio_pin_direction    |= 1 << pin_number;
				break;


			// Handle our most complex mode -- bidirectional uses slices to set the direction
			// of pins on the relevant bus.
			case SGPIO_MODE_STREAM_BIDIRECTIONAL:
				pr_error("sgpio: bidirectional buffer support is not yet implemented!\n");
				break;
		}
	}
}

/**
 * Set up any output pins relevant to the given SGPIO block.
 */
static void sgpio_set_up_output_pins(sgpio_t *sgpio)
{
	for (unsigned i = 0; i < sgpio->function_count; ++i) {
		sgpio_set_up_output_pins_for_function(sgpio, &sgpio->functions[i]);
	}
}


/**
 * Sets up an SGPIO instance to run a provided set of functions.
 *
 * @param sgpio An SGPIO instance object with its functions array already pre-defined. This array specifies how the
 *     SGPIO hardware will be used to interface with the SGPIO pins.
 * @return 0 on success, or an error code on failure.
 */
int sgpio_set_up_functions(sgpio_t *sgpio)
{
	interrupt_service_routine_t data_isr;
	unsigned int optimization_passes = 0;

	bool buffer_optimization_complete = false;
	int rc;

	// First, ensure our SGPIO object has a reference to the right register bank.
	sgpio->reg = platform_get_sgpio_registers();

	// Start off by disabling the shift clock for all SGPIO slices, disabling any previous I/O.
	sgpio->reg->shift_clock_enable = 0UL;

	// Start off with all slices and pins unused.
	sgpio->slices_in_use = 0;
	sgpio->pins_in_use   = 0;

	// Default to using all SGPIO pins as GPIO inputs, which essentially sets them to be not-driven.
	// Also, default them into GPIO "output mode", so they can easily be used as GPIO later.
	pr_debug("sgpio: setting up %" PRIu32 " functions\n", (uint32_t)sgpio->function_count);
	for(unsigned i = 0; i < SGPIO_NUM_PINS; ++i) {
		sgpio->reg->output_configuration[i].pin_direction_source = SGPIO_OUTPUT_MODE_GPIO;
		sgpio->reg->output_configuration[i].pin_direction_source = SGPIO_USE_PIN_DIRECTION_REGISTER;
	}
	for (unsigned i = 0; i < SGPIO_NUM_PINS; ++i) {
		sgpio->reg->sgpio_pin_direction = 0;
	}

	// Perform an initial set-up of each of our SGPIO functions.
	// This sets up each function to perform its function with a minimal amount of used hardware.
	// We'll expand this out after each function is set up.
	for (unsigned i = 0; i < sgpio->function_count; ++i) {
		pr_debug("sgpio: setting up function %u\n", i);
		rc = sgpio_set_up_function(sgpio, &sgpio->functions[i]);

		// Fail out if we can't apply any of our SGPIO configurations.
		if (rc) {
			pr_error("error: sgpio: could not apply function %d (%d)!\n", i, rc);
			return rc;
		}
	}

	// Repeatedly try to optimize our buffer usage until we can't any longer.
	pr_debug("sgpio: functions applied... optimizing...\n");
	while (!buffer_optimization_complete) {
		++optimization_passes;
		buffer_optimization_complete = sgpio_attempt_buffer_optimization(sgpio);
	}
	pr_debug("sgpio: optimization complete in %u passes\n", optimization_passes);

	// Set up each pin for any output functions it may serve.
	pr_debug("sgpio: configuring output pins\n");
	sgpio_set_up_output_pins(sgpio);

	// Finally, generate the ISR that shuttles data around.
	pr_debug("sgpio: generating our data-handling ISR\n");
	data_isr = sgpio_generate_data_shuttle_isr(sgpio);
	pr_debug("sgpio: ISR generation complete.");

	// If we have a data ISR, install it as our interrupt handler.
	if(data_isr) {
		platform_set_interrupt_handler(SGPIO_IRQ, data_isr);
		vector_table.irqs[SGPIO_IRQ] = data_isr;
	}

	return 0;
}

// TODO: implement these!


/**
 * Runs a given SGPIO function, triggering it to shift.
 */
void sgpio_run(sgpio_t *sgpio)
{
	bool interrupt_required = false;

	// Make sure we're not actively shifting while we handle data prepopulation.
	sgpio->reg->shift_clock_enable = 0;

	// Ensure that any data to be immediately scanned out is pre-populated into the data registers.
	sgpio_handle_data_prepopulation(sgpio);

	// Enable the exchange clock interrupt for each I/O slice, if required.
	for (unsigned i = 0; i < sgpio->function_count; ++i) {
		if (sgpio_isr_necessary_for_function(&sgpio->functions[i])) {
			sgpio->reg->exchange_clock_interrupt.set = 1 << sgpio->functions[i].io_slice;
			interrupt_required = true;
		}
	}

	// Enable the SGPIO interrupt, if it's used.
	if (interrupt_required) {
		platform_mark_interrupt_serviced(SGPIO_IRQ);
		platform_enable_interrupt(SGPIO_IRQ);
	}

	sgpio_dump_configuration(LOGLEVEL_INFO, sgpio, false);

	sgpio->reg->shift_clock_enable = sgpio->slices_in_use;
	sgpio->running = true;
}

void sgpio_halt(sgpio_t *sgpio)
{
	sgpio->reg->shift_clock_enable = 0;

	// Disable all SGPIO exchange interrupts.
	sgpio->reg->exchange_clock_interrupt.clear = 0xFFFF;
	platform_disable_interrupt(SGPIO_IRQ);

	sgpio->running = false;
}
