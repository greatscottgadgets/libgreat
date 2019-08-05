/*
 * SGPIO Driver for the LPC43xx series: debug functions.
 *
 * This file is part of libgreat.
 */

#include <toolchain.h>

#include <debug.h>
#include <errno.h>

#include <drivers/scu.h>
#include <drivers/sgpio.h>
#include <drivers/platform_clock.h>

// Private "imports" from sgpio.c/sgpio_isr.c.
int sgpio_slice_for_io(uint8_t pin);
int sgpio_io_pin_for_slice(uint8_t slice);
int sgpio_slice_for_clockgen(uint8_t pin);
bool sgpio_isr_necessary_for_function(sgpio_function_t *function);


/**
 * @return the I/O slice index that controls direction for the given pin and mode
 */
static uint8_t sgpio_io_slice_for_direction_control(uint8_t pin_number, uint32_t direction_source)
{
	const uint8_t single_bit_slices[] = {
		SGPIO_SLICE_B, SGPIO_SLICE_M, SGPIO_SLICE_G, SGPIO_SLICE_N,
		SGPIO_SLICE_D, SGPIO_SLICE_O, SGPIO_SLICE_H, SGPIO_SLICE_P,
		SGPIO_SLICE_A, SGPIO_SLICE_I, SGPIO_SLICE_E, SGPIO_SLICE_J,
		SGPIO_SLICE_C, SGPIO_SLICE_K, SGPIO_SLICE_F, SGPIO_SLICE_L
	};
	const uint8_t multi_bit_slices_2b[] = {
		SGPIO_SLICE_H, SGPIO_SLICE_D, SGPIO_SLICE_G, SGPIO_SLICE_O,
		SGPIO_SLICE_P, SGPIO_SLICE_B, SGPIO_SLICE_N, SGPIO_SLICE_M
	};
	const uint8_t multi_bit_slices_4b[] = {
		SGPIO_SLICE_H, SGPIO_SLICE_O, SGPIO_SLICE_P, SGPIO_SLICE_N,
	};
	const uint8_t multi_bit_slices_8b[] = {
		SGPIO_SLICE_H, SGPIO_SLICE_P,
	};

	switch (direction_source) {

		// In single-bit mode, we do a trivial look up from our look-up-table.
		case SGPIO_DIRECTION_MODE_1BIT: return single_bit_slices[pin_number];

		// For multi-bit modes, we look up offsets in the multi-bit table. For the 2-bit mode,
		// each set of two pins gets a single definition, so we divide by two. For the 4/8 bit,
		// we do a similar thing, but we also mask off a corresponding number of LSBs to get the
		// datasheet Table 275.
		case SGPIO_DIRECTION_MODE_2BIT: return multi_bit_slices_2b[pin_number / 2];
		case SGPIO_DIRECTION_MODE_4BIT: return multi_bit_slices_4b[pin_number / 4];
		case SGPIO_DIRECTION_MODE_8BIT: return multi_bit_slices_8b[pin_number / 8];
	}

	return -1;
}

/**
 * @return the width, in bits, of the bus associated with the given output mode
 */
static uint8_t sgpio_bus_width_for_output_mode(uint32_t output_bus_mode)
{
	switch (output_bus_mode) {
		case SGPIO_OUTPUT_MODE_1BIT:      return 1; break;
		case SGPIO_OUTPUT_MODE_2BIT_A:    return 2; break;
		case SGPIO_OUTPUT_MODE_2BIT_B:    return 2; break;
		case SGPIO_OUTPUT_MODE_2BIT_C:    return 2; break;
		case SGPIO_OUTPUT_MODE_4BIT_A:    return 4; break;
		case SGPIO_OUTPUT_MODE_4BIT_B:    return 4; break;
		case SGPIO_OUTPUT_MODE_4BIT_C:    return 4; break;
		case SGPIO_OUTPUT_MODE_8BIT_A:    return 8; break;
		case SGPIO_OUTPUT_MODE_8BIT_B:    return 8; break;
		case SGPIO_OUTPUT_MODE_8BIT_C:    return 8; break;
		case SGPIO_OUTPUT_MODE_CLOCK_OUT: return 1; break;
		case SGPIO_OUTPUT_MODE_GPIO:      return 1; break;
	}

	return -1;
}


static uint8_t sgpio_io_slice_for_bus_pin(uint8_t pin, uint8_t bus_width)
{
	uint8_t first_pin_in_bus = 0;

	// Figure out the first pin in the bus, which depends on the bus width...
	switch(bus_width) {
		case 1: first_pin_in_bus = pin & ~0b000; break;
		case 2: first_pin_in_bus = pin & ~0b001; break;
		case 4: first_pin_in_bus = pin & ~0b011; break;
		case 8: first_pin_in_bus = pin & ~0b111; break;
	}

	// ... and get the I/O slice for the first pin in the bus.
	return sgpio_slice_for_io(first_pin_in_bus);
}


/**
 * @returns the SGPIO pin that's previous in the concatenation for a given slice; assumes no
 *    wraparound. Returns invalid responses for impossible concatinations.
 */
int sgpio_input_slice_for_concatenation(uint8_t slice, uint8_t depth)
{
	uint8_t previous_pin      = sgpio_io_pin_for_slice(slice) - 1;
	uint8_t preserved_bitmask = sgpio_io_pin_for_slice(slice) & ~(depth - 1);
	uint8_t relevant_pin      = (previous_pin % depth) | preserved_bitmask;

	return sgpio_slice_for_io(relevant_pin);
}



void sgpio_dump_pin_configuration(loglevel_t loglevel, sgpio_t *sgpio, uint8_t pin)
{
	bool has_out;

	loglevel_t continued = loglevel | LOG_CONTINUE;

	// Get a reference to the output configuration regster, which we'll use to generate our raw output.
	volatile sgpio_output_config_register_t *output_config = &sgpio->reg->output_configuration[pin];

	// Grab some general properties of the output pin.
	uint8_t bus_width = sgpio_bus_width_for_output_mode(output_config->output_bus_mode);
	uint8_t position_in_bus = pin % bus_width;

	printk(loglevel,  "    SGPIO%2u: ", (unsigned)pin);

	// If we're using the GPIO register to set the SGPIO pin direction, print that direction.
	if (output_config->pin_direction_source == SGPIO_USE_PIN_DIRECTION_REGISTER) {
		has_out = sgpio->reg->sgpio_pin_direction & (1 << pin);
		printk(continued, has_out ? " OUTPUT" : " INPUT ");

		// Add some spacing so we align with bidirectional pin modes.
		printk(continued, "                       ");

	}
	// Otherwise, print that the pin is bidirectional.
	else {
		has_out = true;

		printk(continued, " BIDIR ");

		// If our direction source isn't the pin direction register, print the source.
		bool is_first_pin = (pin % sgpio_bus_width_for_output_mode(output_config->output_bus_mode)) == 0;

		printk(continued, "   direction source: ");
		printk(continued, "%c", 'A' + sgpio_io_slice_for_direction_control(pin, output_config->pin_direction_source));
		printk(continued, is_first_pin ? "0" :"1");
	}


	if (has_out) {

		// Print the output bus mode.
		printk(continued, "   mode: ");
		switch (output_config->output_bus_mode) {
			case SGPIO_OUTPUT_MODE_1BIT:      printk(continued, "1-bit  "); break;
			case SGPIO_OUTPUT_MODE_2BIT_A:    printk(continued, "2-bit A"); break;
			case SGPIO_OUTPUT_MODE_2BIT_B:    printk(continued, "2-bit B"); break;
			case SGPIO_OUTPUT_MODE_2BIT_C:    printk(continued, "2-bit C"); break;
			case SGPIO_OUTPUT_MODE_4BIT_A:    printk(continued, "4-bit A"); break;
			case SGPIO_OUTPUT_MODE_4BIT_B:    printk(continued, "4-bit B"); break;
			case SGPIO_OUTPUT_MODE_4BIT_C:    printk(continued, "4-bit C"); break;
			case SGPIO_OUTPUT_MODE_8BIT_A:    printk(continued, "8-bit A"); break;
			case SGPIO_OUTPUT_MODE_8BIT_B:    printk(continued, "8-bit B"); break;
			case SGPIO_OUTPUT_MODE_8BIT_C:    printk(continued, "8-bit C"); break;
			case SGPIO_OUTPUT_MODE_GPIO:      printk(continued, "GPIO   "); break;
			case SGPIO_OUTPUT_MODE_CLOCK_OUT: printk(continued, "CLKOUT "); break;
		}

		// Print the source of the binary values to be output on the relevant pin.
		printk(continued, "   source: ");
		switch (output_config->output_bus_mode) {

			// In the 1-bit modes, or any of the A modes, the output source comes directly from the pin's I/O slice.
			case SGPIO_OUTPUT_MODE_1BIT:
			case SGPIO_OUTPUT_MODE_2BIT_A:
			case SGPIO_OUTPUT_MODE_4BIT_A:
			case SGPIO_OUTPUT_MODE_8BIT_A:
				printk(continued, "%c%u", 'A' + sgpio_io_slice_for_bus_pin(pin, bus_width), position_in_bus);
				break;


			// In the 2C/4C modes, the output source comes from the pin that _feeds_ the pin's I/O slice.
			case SGPIO_OUTPUT_MODE_2BIT_C:
			case SGPIO_OUTPUT_MODE_4BIT_C: {
				uint8_t io_slice = sgpio_io_slice_for_bus_pin(pin, bus_width);
				printk(continued, "%c%u", 'A' + sgpio_input_slice_for_concatenation(io_slice, bus_width), position_in_bus);
				break;
			}

			// Mode 8C has a bit of a quirk: to avoid using the direction registers DHOP, it uses a different slice pattern.
			// To simplify things, we'll just print these directly.
			case SGPIO_OUTPUT_MODE_8BIT_C: {
				char slice = (pin >= 8) ? 'N' : 'L';
				printk(continued, "%c%u", slice, position_in_bus);
				break;
			}


			case SGPIO_OUTPUT_MODE_2BIT_B:
			case SGPIO_OUTPUT_MODE_4BIT_B:
			case SGPIO_OUTPUT_MODE_8BIT_B:
				printk(continued, "??");
				break;

			case SGPIO_OUTPUT_MODE_CLOCK_OUT: {
				uint8_t io_slice = sgpio_slice_for_clockgen(pin);
				printk(continued, "%c_clk", 'A' + io_slice);
				break;
			}
		}
	}

	printk(continued, "\n");
}


void sgpio_dump_slice_configuration(loglevel_t loglevel, sgpio_t *sgpio, uint8_t slice)
{
	volatile sgpio_shift_config_register_t *shift_config = &sgpio->reg->shift_configuration[slice];
	volatile sgpio_feature_control_register_t *feature_control = &sgpio->reg->feature_control[slice];

	loglevel_t continued = loglevel | LOG_CONTINUE;

	printk(loglevel,  "    slice[%2u] / %c: ", (unsigned)slice, 'A' + slice);

	// Print parallel information.
	if (feature_control->parallel_mode) {
		printk(continued,  "%" PRIu32 "-bit parallel", 1 << feature_control->parallel_mode);
	}
	else {
		printk(continued,  "serial        ");
	}

	// Print concatenation information:
	if (shift_config->enable_concatenation) {
		uint32_t depth = 1 << shift_config->concatenation_order;
		uint8_t concat_input_slice = sgpio_input_slice_for_concatenation(slice, depth);
		printk(continued,  "    input: slice %c (chain %" PRIu32" deep)", 'A' + concat_input_slice, depth);
	}
	else {
		printk(continued,  "    input: external pin          ");
	}

	// Print clocking information.
	if (feature_control->use_nonlocal_clock) {
		printk(continued,  "   clock on: %s of ", feature_control->shift_on_falling_edge ? "FE" : "RE");
		if (shift_config->use_external_clock) {
			printk(continued,  "pin SGPIO%" PRIu32, shift_config->clock_source_pin + 8);
		} else {
			uint8_t slice_index = (shift_config->clock_source_slice * 4) + 3;
			printk(continued,  "slice %c", 'A' + slice_index);
		}
	} else {
		printk(continued,  "   clock: counter, div: %3u/%3u",
			sgpio->reg->cycle_count[slice], sgpio->reg->sgpio_cycles_per_shift_clock[slice] + 1);
	}

	// Print qualification information.
	if (shift_config->shift_qualifier_mode == SGPIO_ALWAYS_SHIFT_ON_SHIFT_CLOCK) {
		printk(continued,  "   shifts: always");
	} else {
		uint32_t qualifier_type = (shift_config->shift_qualifier_mode << SGPIO_QUALIFIER_TYPE_SHIFT);

		if(qualifier_type == SGPIO_QUALIFIER_TYPE_PIN) {
			printk(continued,  "   shifts iff: %s", feature_control->invert_shift_qualifier ? "!" : "");
			printk(continued,  " SGPIO%" PRIu32, shift_config->shift_qualifier_pin + 8);
		}
		else if(qualifier_type == SGPIO_QUALIFIER_TYPE_SLICE) {
			printk(continued,  "   shifts when: slice [reg val %" PRIu32 "]", shift_config->shift_qualifier_slice);
		}
		else {
			printk(continued,  "   -INVALID QUALIFIER-");
		}
	}

	// Print information on the data/shadow register swaps.
	if (sgpio->reg->stop_on_next_buffer_swap & (1 << slice)) {
		printk(continued, "   operates for %3d shifts (shadow unused)",
			sgpio->reg->data_buffer_swap_control[slice].shifts_remaining + 1);
	} else {
		printk(continued, "   data/shadow swap every %3d shifts ",
			sgpio->reg->data_buffer_swap_control[slice].shifts_per_buffer_swap + 1);

		// Indicate if the given data/shadow swap uses an IRQ.
		if (sgpio->swap_irqs_required & (1 << slice)) {
			printk(continued, " [IRQ]");
		}
	}

	printk(continued, "\n");
}


void sgpio_dump_function_info(loglevel_t loglevel, sgpio_t *sgpio, int function_index)
{
	loglevel_t continued = loglevel | LOG_CONTINUE;
	sgpio_function_t *function = &sgpio->functions[function_index];

	printk(loglevel,  "    function %u: ",  function_index);

	// Print the function's mode.
	switch(function->mode) {
		case SGPIO_MODE_STREAM_DATA_IN:                 printk(continued, "STREAM IN "); break;
		case SGPIO_MODE_STREAM_DATA_OUT:                printk(continued, "STREAM OUT"); break;
		case SGPIO_MODE_FIXED_DATA_OUT:                 printk(continued, "FIXED OUT "); break;
		case SGPIO_MODE_CLOCK_GENERATION:               printk(continued, "CLOCKGEN  "); break;
		case SGPIO_MODE_STREAM_BIDIRECTIONAL:           printk(continued, "BIDIR BUS "); break;
		default:                                        printk(continued, "INVALID   "); break;
	}

	printk(continued, "  io slice: %c / %2u", function->io_slice + 'A', function->io_slice);
	printk(continued, "  buffer order/size: %u/%u", function->buffer_order, 1 << function->buffer_order);
	printk(continued, "  buffer position: %u", function->position_in_buffer);

	if (function->mode == SGPIO_MODE_STREAM_BIDIRECTIONAL) {
		printk(continued, "  direction slice: %c / %2u", function->io_slice + 'A', function->direction_slice);
		printk(continued, "  direction buffer order/size: %u/%u",
			function->direction_buffer_order, 1 << function->direction_buffer_order);
		printk(continued, "  direction buffer position: %u", function->position_in_direction_buffer);
	}

	// TODO: print more information here

	printk(continued, "\n");
}


void sgpio_dump_slice_contents(loglevel_t loglevel, sgpio_t *sgpio, uint8_t slice)
{
	loglevel_t continued = loglevel | LOG_CONTINUE;

	printk(loglevel,  "    slice[%2u] / %c: ", (unsigned)slice, 'A' + slice);
	printk(continued, "    data: %08x", sgpio->reg->data[slice]);
	printk(continued, "    shadow: %08x", sgpio->reg->data_shadow[slice]);
	printk(continued, "\n");
}


/**
 * Arguments used by our machine code.
 * (Were these hardcoded, most of these would have been added to the literal pool,
 *  so this doesn't even affect optimization; which is nice.)
 */
typedef struct ATTR_PACKED {

	//
	// NOTE: If you re-order these, you _must_ update the corresponding constants in sgpio.S!
	//

	uint32_t  interrupt_clear_mask;
	uint32_t  copy_size;

	void      *buffer;
	uint32_t  *position_in_buffer_var;
	uint32_t  position_in_buffer_mask;
	uint32_t  *data_in_buffer_var;

} sgpio_isr_arguments_t;

// Get a reference to our argument structure, for the code below.
extern sgpio_isr_arguments_t sgpio_dynamic_isr_args;


void sgpio_dump_configuration(loglevel_t loglevel, sgpio_t *sgpio, bool include_unused)
{
	// Print a header.
	printk(loglevel, "--- SGPIO state dump (%d functions) --- \n", sgpio->function_count);
	printk(loglevel, "======================== \n");

	printk(loglevel, "\n");
	printk(loglevel, "Software function configuration: \n");
	for (unsigned i = 0; i < sgpio->function_count; ++i) {
		sgpio_dump_function_info(loglevel, sgpio, i);
	}


	printk(loglevel, "\n");
	printk(loglevel, "Pin configuration: \n");
	printk(loglevel, "    pin usage mask: 0x%04x\n", sgpio->pins_in_use);
	printk(loglevel, "    GPIO output enable: %04x" "\n", sgpio->reg->sgpio_pin_direction);
	for (unsigned i = 0; i < SGPIO_NUM_PINS; ++i) {
		uint16_t pin_mask = 1 << i;

		if (include_unused || (pin_mask & sgpio->pins_in_use)) {
			sgpio_dump_pin_configuration(loglevel, sgpio, i);
		}

	}

	printk(loglevel, "\n");
	printk(loglevel, "Slice configuration: \n");
	printk(loglevel, "    slice usage mask: 0x%04x\n", sgpio->slices_in_use);
	for (unsigned i = 0; i < SGPIO_NUM_SLICES; ++i) {
		uint16_t slice_mask = (1 << i);

		// If the slice is in use, or we're printing unused slices as well, dump its data.
		if (include_unused || (slice_mask & sgpio->slices_in_use)) {
			sgpio_dump_slice_configuration(loglevel, sgpio, i);
		}
	}


	printk(loglevel, "\n");
	printk(loglevel, "Slice contents: \n");
	for (unsigned i = 0; i < SGPIO_NUM_SLICES; ++i) {
		uint16_t slice_mask = (1 << i);

		// If the slice is in use, or we're printing unused slices as well, dump its data.
		if (include_unused || (slice_mask & sgpio->slices_in_use)) {
			sgpio_dump_slice_contents(loglevel, sgpio, i);
		}
	}
}

#define GET_RAW_REGISTER(name) \
	*((uint32_t*)((uintptr_t)platform_get_sgpio_registers() + offsetof(platform_sgpio_registers_t, name)))
#define DUMP_REGISTER(name) \
	printk(loglevel, "%s: %08\n" PRIu32, #name, GET_RAW_REGISTER(name))


void sgpio_dump_registers(loglevel_t loglevel, sgpio_t *sgpio)
{
	printk(loglevel, "--- SGPIO register dump --- \n");
	printk(loglevel, "======================== \n");

	DUMP_REGISTER(shift_configuration[0]);
	DUMP_REGISTER(shift_configuration[1]);
	DUMP_REGISTER(shift_configuration[2]);
	DUMP_REGISTER(shift_configuration[3]);
	DUMP_REGISTER(shift_configuration[4]);
	DUMP_REGISTER(shift_configuration[5]);
	DUMP_REGISTER(shift_configuration[6]);
	DUMP_REGISTER(shift_configuration[7]);
	DUMP_REGISTER(shift_configuration[8]);

}
