/*
 * SGPIO IRQ generation meta-code
 *
 * This file is part of libgreat.
 */


#include <toolchain.h>
#include <string.h>

#include <debug.h>
#include <errno.h>

#include <drivers/sgpio.h>
#include <drivers/arm_vectors.h>

// FIXME: this currently sets up a single ISR, but that's not appropriate for any setup where different slices
// can update at different rates. We should assume the template is _copied_ each time, so we can set up multiple
// handlers for multiple different events. This is still a TODO, but it needs to happen soon to maintain this driver's
// genericism.

//#define pr_debug pr_info

// Normally, branching is expensive, so we'll try to avoid it. Comment this out to branch past small sets
// of unused instructions; which is probably slower.
#define CONFIG_ISR_GENERATION_AVOID_BRANCHING

// From the main body of the SGPIO driver -- expose some of is internal API we want to keep out of the header.
int sgpio_slice_for_io(uint8_t pin);
int sgpio_io_pin_for_slice(uint8_t slice);

/**
 * Bitfield structure that describes an THUMB load/store with immediate offset.
 * Used to generate machine code.
 */
typedef union {

	// Bitfield describing each part of the instruction...
	struct {
		uint16_t rd      : 3;
		uint16_t rb      : 3;
		uint16_t offset  : 5;
		uint16_t is_load : 1;
		uint16_t opcode  : 4; // should be 0110 for word or 0111 for byte
	};

	// ... and a value describing the whole thing.
	uint16_t instruction;

} register_offset_instruction_t;


//.equ SGPIO_CLEAR,                    0x00
//.equ SGPIO_COPY_SIZE,                0x01
//.equ SGPIO_BUFFER_ADDRESS,           0x02
//.equ SGPIO_BUFFER_OFFSET_ADDRESS,    0x03
//.equ SGPIO_BUFFER_OFFSET_MASK,       0x04
//.equ SGPIO_FILL_COUNT_ADDRESS,       0x05

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


/**
 * Bitfield structure that describes an THUMB unconditional branch.
 * Used to generate machine code.
 */
typedef union {

	// Bitfield describing each part of the instruction...
	struct {
		uint16_t offset  : 11;
		uint16_t opcode  :  5; // should be 11100
	};

	// ... and a value describing the whole thing.
	uint16_t instruction;

} unconditional_branch_instruction_t;


/**
 * Constants that match up with our core ASM.
 */
enum {
	// Machine code constants.
	OPCODE_LDR_STR                    = 0x06,
	OPCODE_BRANCH                     = 0x1c,
	OPCODE_NOP                        = 0xbf00,

	// References to the registers we use in the template.
	REG_SCRATCH                       = 0,
	REG_SHADOW_BASE                   = 1,
	REG_USER_BUFFER_POINTER           = 2,
};


/**
 * Reference to the literal pool in our ISR template.
 */
extern uint32_t sgpio_dynamic_isr_literal_pool[];


/**
 * Reference to the body of our ISR template, where most of the copies happen.
 */
extern uint16_t sgpio_dynamic_isr_body[];
extern uint16_t sgpio_dynamic_isr_end[];


/**
 * References to various points in our assembly template.
 */
extern uint16_t sgpio_dynamic_isr_update_buffer_a;
extern uint16_t sgpio_dynamic_isr_update_buffer_b;
extern uint16_t sgpio_dynamic_isr_return;


// Get a reference to our SGPIO dynamic ISR.
void sgpio_dynamic_isr(void);


/**
 * @returns the machine code for {ldr, str} rd, [rb, #offset].
 * @param is_load Determines if this should return LDR (true) or STR (false).
 */
static uint16_t _ldr_or_str(uint16_t rd, uint16_t rb, uint16_t offset, uint16_t is_load)
{
	uint16_t offset_in_words = offset >> 2;

	register_offset_instruction_t machine_code = {
		.rd = rd,
		.rb = rb,
		.offset = offset_in_words,
		.opcode = OPCODE_LDR_STR,
		.is_load = is_load
	};
	return machine_code.instruction;
}

/**
 * @returns the machine code for ldr rd, [rb, #offset].
 */
static uint16_t _ldr(uint16_t rd, uint16_t rb, uint16_t offset)
{
	return _ldr_or_str(rd, rb, offset, true);
}


/**
 * @returns the machine code for str rd, [rb, #offset].
 */
static uint16_t _str(uint16_t rd, uint16_t rb, uint16_t offset)
{
	return _ldr_or_str(rd, rb, offset, false);
}


/**
 * @returns the machine code for a NOP
 */
static uint16_t _nop(void)
{
	return OPCODE_NOP;
}


/**
 * @returns the machine code for an unconditional branch.
 */
static uint16_t _branch(uint16_t offset)
{
	unconditional_branch_instruction_t machine_code = {
		.offset = offset,
		.opcode = OPCODE_BRANCH,
	};
	return machine_code.instruction;
}


/**
 * @return the slice number that corresponds to the given position in the concatenated "direction slice" buffer
 */
static uint32_t sgpio_get_direction_buffer_slice_index(sgpio_function_t *function, uint32_t position_in_buffer)
{
	uint32_t position_to_look_up = position_in_buffer;

	// If we have more than one slice, offset our position by one.
	// This accounts for the fact that our slices drive the output using values that are a word in.
	// TODO: validate this math?
	/*/
	if(function->buffer_depth_order) {
		position_to_look_up = (position_in_buffer + 1) % direction_bus_width
	}
	*/

	return sgpio_slice_for_io(sgpio_io_pin_for_slice(function->direction_slice) + position_to_look_up);
}



/**
 * @return the slice number that corresponds to the given position in the concatenated "slice" buffer
 */
static uint32_t sgpio_get_function_buffer_slice_index(sgpio_function_t *function, uint32_t position_in_buffer)
{
	uint32_t position_to_look_up = position_in_buffer;
	uint32_t buffer_depth_slices = 1 << function->buffer_depth_order;

	switch (function->mode) {

		// Modes with output.
		case SGPIO_MODE_STREAM_DATA_OUT:
		case SGPIO_MODE_FIXED_DATA_OUT:

		// FIXME: does this need its own case?
		case SGPIO_MODE_STREAM_BIDIRECTIONAL:

			// If we have more than one slice, offset our position by one.
			// This accounts for the fact that our slices drive the output using values that are a word in.
			// TODO: validate this math?
			if(function->buffer_depth_order) {
				position_to_look_up = (position_in_buffer + 1) % buffer_depth_slices;
			}
			// Falls through.

		// Pure-input modes.
		case SGPIO_MODE_STREAM_DATA_IN:
			return sgpio_slice_for_io(sgpio_io_pin_for_slice(function->io_slice) + position_to_look_up);
		default:
			pr_error("sgpio: could not figure out slice layout for this mode! cannot handle buffering!\n");
			return -1;
	}
}


/**
 * @return the offset into the SGPIO function or shadow registers that corresponds to the given position
 *    in the concatenated "slice" buffer
 */
static uint32_t sgpio_get_function_buffer_offset(sgpio_function_t *function, uint32_t position_in_buffer)
{
	return sgpio_get_function_buffer_slice_index(function, position_in_buffer) * sizeof(uint32_t);
}


/**
 * Generates the machine code that performs the actual copies for the given ISR.
 */
static int generate_isr_copy_instructions(sgpio_function_t *function,
	unsigned *position_in_instruction_body, unsigned *position_in_user_buffer)
{
	uint16_t *instruction_buffer = sgpio_dynamic_isr_body;

	unsigned instruction_offset = *position_in_instruction_body;
	unsigned user_buffer_offset = *position_in_user_buffer;

	// Figure out how many slices we want to copy-- this is essentially the same as how deep our buffer is, in slices.
	uint8_t slices_to_copy = 1 << function->buffer_depth_order;

	// Create a set of instructions for each of our slices to copy.
	while (slices_to_copy--) {
		uint32_t slice_buffer_offset;

		// Generate the instructions themselves, which vary depending on our mode.
		switch (function->mode) {

			// XXX: temporary hack; this should have its own ISR type
			case SGPIO_MODE_STREAM_BIDIRECTIONAL:

			case SGPIO_MODE_STREAM_DATA_OUT:
			case SGPIO_MODE_FIXED_DATA_OUT:
				// Determine which slice we want to read data from, at the current point....
				slice_buffer_offset = sgpio_get_function_buffer_offset(function, slices_to_copy);
				pr_debug("sgpio: ISR: slice %u: copying to offset %u (instruction body position: %u)\n",
					slices_to_copy, slice_buffer_offset, instruction_offset);

				// ... and generate the instrunctions to copy the data from the shadow registers to the user buffer.
				instruction_buffer[instruction_offset++] = _ldr(REG_SCRATCH, REG_USER_BUFFER_POINTER, user_buffer_offset);
				instruction_buffer[instruction_offset++] = _str(REG_SCRATCH, REG_SHADOW_BASE, slice_buffer_offset);
				user_buffer_offset += sizeof(uint32_t);
				break;

			case SGPIO_MODE_STREAM_DATA_IN:
				// Determine which slice we want to read data from, at the current point....
				slice_buffer_offset = sgpio_get_function_buffer_offset(function, slices_to_copy);
				pr_debug("sgpio: ISR: slice %u: copying from offset %u (instruction body position: %u)\n",
					slices_to_copy, slice_buffer_offset, instruction_offset);

				// ... and generate the instrunctions to copy the data from the shadow registers to the user buffer.
				instruction_buffer[instruction_offset++] = _ldr(REG_SCRATCH, REG_SHADOW_BASE, slice_buffer_offset);
				instruction_buffer[instruction_offset++] = _str(REG_SCRATCH, REG_USER_BUFFER_POINTER, user_buffer_offset);
				user_buffer_offset += sizeof(uint32_t);
				break;

			default:
				pr_error("sgpio: cannot yet generate ISRs for this function type (%d)!\n", function->mode);
				return -ENOSYS;

		}
	}

	*position_in_user_buffer = user_buffer_offset;
	*position_in_instruction_body = instruction_offset;

	// Finally, indicate success.
	return 0;
}

/**
 * Adds a branch in the instruction stream that branches past any unused instructions.
 */
static void generate_branch_past_unused_instructions(unsigned position_in_instruction_body)
{
	uint32_t branch_offset, branch_target;

	uint16_t *instruction_buffer = sgpio_dynamic_isr_body;
	uint32_t current_instruction_address = (uint32_t)&instruction_buffer[position_in_instruction_body];

	// Compute the branch base, which is equal to the value of the PC accounting for prefetching -- so the
	// current instruction's address + 4.
	uint32_t prefetched_pc = current_instruction_address + sizeof(uint32_t);


	// Compute the branch offset for the relevant branch instruction...
	branch_target = (uint32_t)sgpio_dynamic_isr_end;
	branch_offset = (branch_target >> 1) - (prefetched_pc >> 1);

	// ... and write that branch instruction into our instruction stream.
	instruction_buffer[position_in_instruction_body] = _branch(branch_offset);
}

/**
 * @returns the length (in instructions) of the space we can fill with user-generated instructions
 */
static unsigned sgpio_get_maximum_generable_instructions(void)
{
	// Figure out how many bytes are in the SGPIO body.
	uint32_t bytes_in_body = (uint32_t)sgpio_dynamic_isr_end - (uint32_t)sgpio_dynamic_isr_body;

	// Break this up into the amount of (16-bit) thumb insturctions that will fit in the relevant body.
	return bytes_in_body / sizeof(uint16_t);
}


/**
 * @return true iff the function's data buffer fits in the relevant slices
 */
bool sgpio_data_buffer_fits_in_sgpio_slice_chain(sgpio_function_t *function, bool with_exchange)
{
	uint8_t slice_buffer_order_bytes         = function->buffer_depth_order + 2;
	uint8_t slice_buffer_order_with_exchange = slice_buffer_order_bytes + 1;

	// Compute how many shifts can occur per slice register in our buffer...
	uint32_t slices_in_chain = 1 << function->buffer_depth_order;
	uint8_t shifts_per_slice = 32 / function->bus_width;

	// ... and compute the total shifts per slice chain ("per data/shadow swap").
	uint8_t shifts_total = shifts_per_slice * slices_in_chain;

	// If we're including the shadow registers in our consideration of the slice chain,
	// double the amount of total shifts we expect.
	if (with_exchange) {
		shifts_total *= 2;
	}

	// If we have any function with a shift limitation that's less than the shifts that'd occur
	// before we'd need to reload the data, the relevant data will always fit in the slice chain.
	if (function->shift_count_limit && (function->shift_count_limit < shifts_total)) {
		return true;
	}

	// If we're not in fixed output mode, and we're not limited as above, we expect new data to
	// come in constantly. This data will keep coming in; so we definitely can't fit all of it in
	// our slice chain.
	if (function->mode != SGPIO_MODE_FIXED_DATA_OUT) {
		return true;
	}

	// If we are in fixed output mode, we'll need to compare the size of the data with the actual
	// size of our slice chain to see if it fits.
	if (with_exchange) {
		return function->buffer_order <= slice_buffer_order_with_exchange;
	} else {
		return function->buffer_order <= slice_buffer_order_bytes;
	}
}


/**
 * Method that tries to avoid an ISR, whenever possible.
 */
bool sgpio_isr_necessary_for_function(sgpio_function_t *function)
{
	// If user configuration is preventing us from generating an ISR, return that
	// no ISR is necessary; and assume the user knows what they're doing.
	if (function->overrides & SGPIO_FUNCTION_OVERRIDE_NEVER_USE_ISR) {
		return false;
	}

	switch (function->mode) {

		// If we're in clock generation, we can implicitly avoid an ISR.
		case SGPIO_MODE_CLOCK_GENERATION:
			return false;

		// FIXME: implement an interrupt handler for bidirectional functions
		case SGPIO_MODE_STREAM_BIDIRECTIONAL:

		// If we're streaming fixed data out, we can avoid an ISR as long as we can fit the entire
		// pattern in our streaming buffer. That means as long as our user buffer's order (in bytes) is
		// less than the total order we have between both the shadow and data registers, we can just shift
		// directly from the slice buffers, and avoid an ISR to re-populate either.
		//
		// If we're streaming live data out, we can avoid an ISR iff the shift count is limited in the
		// function configuration. If so, we can check to see if all of the relevant data will fit in
		// a single slice chain.
		case SGPIO_MODE_STREAM_DATA_OUT:
		case SGPIO_MODE_FIXED_DATA_OUT:
			return !sgpio_data_buffer_fits_in_sgpio_slice_chain(function, true);

		// If we're streaming in, we almost always need an interrupt -- with one exception.
		// If the function's shift limit would have us shift in less data than a full chain,
		// we don't need a "swap" ISR -- we'll handle data capture automatically when sgpio_halt
		// is issued.
		case SGPIO_MODE_STREAM_DATA_IN:
			if (function->shift_count_limit) {
				uint32_t shift_limit_bytes = (function->shift_count_limit * function->bus_width) / SGPIO_BITS_PER_SLICE;
				uint32_t shift_chain_bytes = (1 << function->buffer_depth_order);

				// If the shift limit would take more than one full chain to execute, we need an ISR.
				return (shift_limit_bytes > shift_chain_bytes);
			}
			// In all other cases, we need an interrupt.
			else {
				return true;
			}

		default:
			return true;
	}

}


/**
 * Method that generates assembly code for the SGPIO data-shuttling ISR. Essentially generates the code used
 * to copy data into and out of the SGPIO shadow buffers.
 */
void *sgpio_generate_core_data_isr_for_function(sgpio_function_t *function)
{
	unsigned position_in_instruction_body = 0;
	unsigned position_in_user_buffer = 0;
	int rc;

	// Optimization: in some modes, we can avoid having an ISR completely.
	// If an ISR isn't necessary, we're trivially done!
	if (!sgpio_isr_necessary_for_function(function)) {
		return NULL;
	}

	//
	// Populate our assembly template's core arguments.
	//

	// Only the function's I/O slice generates interrupts, so it's the only IRQ that needs to be marked as serviced.
	sgpio_dynamic_isr_args.interrupt_clear_mask     = (1 << function->io_slice);

	// XXX
	sgpio_dynamic_isr_args.interrupt_clear_mask     = 0xFFFF;

	// Populate information about the user buffer to processed..
	sgpio_dynamic_isr_args.buffer                   = function->buffer;
	sgpio_dynamic_isr_args.position_in_buffer_var   = &function->position_in_buffer;
	sgpio_dynamic_isr_args.data_in_buffer_var       = &function->data_in_buffer;
	sgpio_dynamic_isr_args.position_in_buffer_mask  = (1 << function->buffer_order) - 1;
	pr_debug("sgpio: ISR: position mask is: 0x%x\n", sgpio_dynamic_isr_args.position_in_buffer_mask);

	// Figure out how much data will be copied during the relevant ISR -- which is just the number of slices used
	// multiplied by the size of each slice (32 bits).
	sgpio_dynamic_isr_args.copy_size = (1 << function->buffer_depth_order) * sizeof(uint32_t);

	// ... build the instructions that perform the copies themselves...
	pr_debug("sgpio: ISR: generating machine code...\n");
	rc = generate_isr_copy_instructions(function, &position_in_instruction_body, &position_in_user_buffer);
	if (rc) {
		return NULL;
	}

	// ... and, finally, handle any unused instructions, if we have any.
	// We want this whenever the instruction stream isn't full -- so when we don't have a full set of 64 instructions,
	// or we don't have a full complement of buffers.
	pr_debug("sgpio: ISR: tidying up any unused instructions...\n");

#ifdef CONFIG_ISR_GENERATION_AVOID_BRANCHING
	(void)generate_branch_past_unused_instructions;

	// NOP out any unused instructions.
	while(position_in_instruction_body < sgpio_get_maximum_generable_instructions()) {
		sgpio_dynamic_isr_body[position_in_instruction_body] = _nop();
		++position_in_instruction_body;
	}
#else
	(void)_nop();

	// Branch past any unused instructions.
	if (position_in_user_buffer != sgpio_get_maximum_generable_instructions()) {
		generate_branch_past_unused_instructions(position_in_instruction_body);
	}
#endif

	pr_debug("sgpio: ISR: generation complete!\n");
	return sgpio_dynamic_isr;
}



/**
 * Method that generates assembly code for the SGPIO data-shuttling ISR. Essentially generates the code used
 * to copy data into and out of the SGPIO shadow buffers.
 */
void *sgpio_generate_isr_for_function(sgpio_function_t *function)
{
	return sgpio_generate_core_data_isr_for_function(function);
}


interrupt_service_routine_t sgpio_generate_master_isr(sgpio_t *sgpio, interrupt_service_routine_t *function_isrs)
{
	// FIXME: implement this function!

	// Temporary stand-in that just returns the first ISR generated; since we currently enforce
	// a single-ISR limit. This should be lifted shortly.
	for (unsigned i = 0; i < sgpio->function_count; ++i) {
		if (sgpio->swap_irqs_required & (1 << i)) {
			return function_isrs[i];
		}
	}

	return NULL;
}


/**
 * Generates an ISR that handles shuttling around SGPIO data.
 */
int sgpio_generate_data_shuttle_isr(sgpio_t *sgpio)
{
	interrupt_service_routine_t function_isrs[16];
	interrupt_service_routine_t master_isr;

	sgpio->swap_irqs_required = 0;

	// FIXME: generate for every _applicable_ function and possibly generate a master ISR to stitch them all together.
	// Currently, this breaks if we have more than one function.

	// Generate ISRs for the functions that require them.
	for (unsigned i = 0; i < sgpio->function_count; ++i) {
		if (sgpio_isr_necessary_for_function(&sgpio->functions[i])) {

			// FIXME: This gate is here to fail out early if we're going to run into
			// not-yet-implemented functionality. It should be removed when we implement
			// calling of multiple ISRs.
			if (sgpio->swap_irqs_required) {
				pr_error("sgpio: support for multiple IRQ-requiring functions is not yet implemented\n");
				pr_error("       bailing out, as we can't meet the relevant constraints\n");
			}


			sgpio->swap_irqs_required = 1 << sgpio->functions[i].io_slice;
			function_isrs[i] = sgpio_generate_isr_for_function(&sgpio->functions[i]);

			// If we didn't wind up with an ISR while one was necessary, something went wrong.
			// Fail out.
			if (!function_isrs[i]) {
				pr_error("sgpio: error: didn't wind up with an ISR when we expected one!\n");
				return EINVAL;
			}
		}
	}

	// Generate the master ISR, which stitches together all of the individual function ISR bodies.
	master_isr = sgpio_generate_master_isr(sgpio, function_isrs);

	// If we have a data ISR, install it as our interrupt handler.
	if(master_isr) {
		platform_set_interrupt_handler(SGPIO_IRQ, master_isr);
	}

	return 0;
}


/**
 * Prepopulates either the data or shadow registers associated with the given function with the next data from
 * the user data buffer, allowing us to scan-out without any "dead" data space.
 */
 // FIXME: handle special cases of buffer depth order = 0, 1; these have values less than uint32_t long
static void sgpio_prepopulate_function_buffer(sgpio_function_t *function, volatile uint32_t *target_registers)
{
	// Get a quick reference to the user buffer, and compute its size in bytes and how far we are into it.
	uint32_t buffer_size_bytes = (1UL << function->buffer_order);

	// Populate the data buffer with the data to be shifted out immediately.
	uint8_t current_word_index  = (1UL << function->buffer_depth_order);
	pr_debug("sgpio: copying %d words into %d byte buffer\n", current_word_index, buffer_size_bytes);
	while (current_word_index--) {

		// We wrap around the circular buffer -- this both allows us to "wrap around" the edge if our current position
		// starts at a location other than zero, and allows us to repeat the buffer multiple times if we have a buffer
		// shorter than the relevant concatinated slice chain.
		uintptr_t position_in_buffer    = (uintptr_t)(function->position_in_buffer % buffer_size_bytes);
		uint32_t *target_buffer_segment = (uint32_t *)((uintptr_t)function->buffer + position_in_buffer);

		// Copy a single slice at a time into the target register set.
		uint32_t slice_buffer_offset          = sgpio_get_function_buffer_slice_index(function, current_word_index);
		target_registers[slice_buffer_offset] = *target_buffer_segment;

		pr_debug("word %d: target_registers[%d] / slice %c = %08x\n", current_word_index,
			slice_buffer_offset, 'A' + (slice_buffer_offset / 4), *target_buffer_segment);

		// And move to the next slice.
		function->position_in_buffer += sizeof(uint32_t);
		function->position_in_buffer = function->position_in_buffer % buffer_size_bytes;
	}
}


/**
 * Prepopulates either the data or shadow registers associated with the given function's direction slices
 * with the next data from the user direction buffer, allowing us to scan-out without any "dead" data space.
 */
static void sgpio_prepopulate_direction_buffer(sgpio_t *sgpio, sgpio_function_t *function, volatile uint32_t *target_registers)
{
	// Get a quick reference to the user buffer, and compute its size in bytes and how far we are into it.
	uint32_t direction_buffer_size_bytes = (1UL << function->direction_buffer_order);

	// Figure out how many shifts happen per data/shadow swap, and derive from that
	// how many bytes of direction data we need per data/shadow swap. This is necessary,
	// as limited data chain length may mean that we don't use the whole data register per swap.
	uint32_t shifts_per_swap = sgpio->reg->data_buffer_swap_control[function->io_slice].shifts_remaining + 1;
	uint32_t bits_per_shift = (function->bus_width == 1) ? 1 : 2;
	uint32_t direction_bytes_per_swap  = (shifts_per_swap * bits_per_shift) / 8;

	// Populate the data buffer with the data to be shifted out immediately.
	uint8_t bytes_remaining  = direction_bytes_per_swap;
	pr_debug("sgpio: direction shifts per swap: %d\n", shifts_per_swap);
	pr_debug("sgpio: direction bits per shift: %d\n", bits_per_shift);
	pr_debug("sgpio: bytes to copy for direction buffer: %d\n", bytes_remaining);
	while (bytes_remaining) {

		// Most iterations copy a full
		uint32_t bytes_copied = sizeof(uint32_t);
		uint32_t current_word_index = bytes_remaining / 4;

		// We wrap around the circular buffer -- this both allows us to "wrap around" the edge if our current position
		// starts at a location other than zero, and allows us to repeat the buffer multiple times if we have a buffer
		// shorter than the relevant concatinated slice chain.
		uintptr_t position_in_buffer    = (uintptr_t)(function->position_in_direction_buffer % direction_buffer_size_bytes);
		uint32_t *target_buffer_segment = (uint32_t *)((uintptr_t)function->direction_buffer + position_in_buffer);

		// Grab the value to write from the target buffer.
		uint32_t data_to_write = *target_buffer_segment;

		// Special case: if we have less than a full word of bytes remaining, expand the remaining bytes into a word,
		// and then populate that.
		if (bytes_remaining < sizeof(uint32_t)) {

			// Truncate our "bytes to copy" variable to the amount of bytes remaining.
			bytes_copied = bytes_remaining;

			// Start off with a slice register value of 0; and copy in however many bytes are remaining.
			data_to_write = 0;

			// .. and populate the rest of that padding byte.
			memcpy(&data_to_write, target_buffer_segment, bytes_copied);
		}

		// Finally, populate the relevant slice with the data to write.
		uint32_t slice_buffer_offset          = sgpio_get_direction_buffer_slice_index(function, current_word_index);
		target_registers[slice_buffer_offset] = data_to_write;

		// And move to the next slice.
		function->position_in_direction_buffer += bytes_copied;
		function->position_in_direction_buffer = function->position_in_buffer % direction_buffer_size_bytes;
		bytes_remaining -= bytes_copied;
	}
}



/**
 * Pre-populates the data/shadow registers for any functions that scan-out data.
 * Should be called once data is ready -- so immediately before run.
 */
void sgpio_handle_data_prepopulation(sgpio_t *sgpio)
{
	for (unsigned i = 0; i < sgpio->function_count; ++i) {
		sgpio_function_t *function = &sgpio->functions[i];

		switch (function->mode) {

			//
			// For bidirectional mode, we'll prepopulate the direction-slice-chain data/shadow registers,
			// and then fall through to populate the I/O-slice-chain data/shadow registers.
			//
			case SGPIO_MODE_STREAM_BIDIRECTIONAL:
				pr_debug("sgpio: pre-populating direction buffer\n");
				sgpio_prepopulate_direction_buffer(sgpio, function, sgpio->reg->data);
				sgpio_prepopulate_direction_buffer(sgpio, function, sgpio->reg->data_shadow);

				// falls through


			//
			// For modes with output, we'll prepopulate the data and shadow registers.
			//
			case SGPIO_MODE_STREAM_DATA_OUT:
			case SGPIO_MODE_FIXED_DATA_OUT:
				pr_debug("sgpio: pre-populating data buffer\n");
				sgpio_prepopulate_function_buffer(function, sgpio->reg->data);
				sgpio_prepopulate_function_buffer(function, sgpio->reg->data_shadow);
				break;

			// For all other modes, we don't need to do anything.
			default:
				break;

		}
	}
}


/**
 * Captures any data remaining in the slice buffers for an individual function after halt.
 */
static void sgpio_capture_remaining_data_for_function(sgpio_t *sgpio, sgpio_function_t *function)
{
	uint32_t shifts_to_process;
	uint32_t bytes_to_process;

	// Figure out the shape of our buffer.
	uint8_t *data_buffer     = function->buffer;
	uint8_t data_buffer_size = (1 << function->buffer_order);
	volatile uint32_t *slice_buffers;

	// Grab the contents of our swap-control register, which tracks our position in the slice chain.
	sgpio_shift_position_register_t swap_position = sgpio->reg->data_buffer_swap_control[function->io_slice];

	// We have two cases: the function halted because we set a limit, or the function halted because
	// we manually halted it.

	// If we have zero "shifts per buffer swap", and zero position, we were halted automatically
	// by our shift limit. We'll need to process data for our full shift limit, and from the _shadow_
	// registers, as the last thing the SGPIO peripheral did before stopping was to swap the buffers.
	if (!swap_position.shifts_per_buffer_swap && !sgpio->reg->cycle_count[function->io_slice]) {
		shifts_to_process = function->shift_count_limit;
		slice_buffers = sgpio->reg->data_shadow;
	}
	// Otherwise, we were stopped manually.
	else {
		// TODO: implement this case!
		slice_buffers = sgpio->reg->data;
		return;
	}

	// Convert figure out how many bytes in the shift chain we'll need to copy.
	bytes_to_process = (shifts_to_process * function->bus_width) / 8;
	pr_debug("sgpio: capturing final %d byte(s) of slice buffers\n", bytes_to_process);

	// Process each of the bytes necessary.
	for (unsigned i = 0; i < bytes_to_process; ++i) {

		// Figure out which slice contains the data for the current byte...
		uint8_t slice_in_chain = i / 4;
		uint8_t slice_index = sgpio_get_function_buffer_slice_index(function, slice_in_chain);

		// ... grab its contents as a word...
		uint32_t slice_data = slice_buffers[slice_index];

		// ... and extract the relevant byte.
		uint32_t data_shift_bytes = 3 - (i % 4); // (maximum index - (position in byte))
		uint8_t data_byte = slice_data >> (data_shift_bytes * 8);

		if (i % sizeof(uint32_t) == 0) {
			pr_debug("sgpio: capturing from slice %c (%02x)\n", slice_index + 'A', data_byte);
		}

		// Finally, copy the byte to the function's data buffer.
		data_buffer[function->position_in_buffer] = data_byte;
		function->position_in_buffer = (function->position_in_buffer + 1) % data_buffer_size;
	}

}



/**
 * Captures any data remaining in the SGPIO buffers upon halt.
 * Used to capture data that would have been grabbed at the next interrupt.
 */
void sgpio_handle_remaining_data(sgpio_t *sgpio)
{
	for (unsigned i = 0; i < sgpio->function_count; ++i) {
		sgpio_function_t *function = &sgpio->functions[i];

		switch (function->mode) {

			//
			// For modes with input, we'll prepopulate the data and shadow registers.
			//
			case SGPIO_MODE_STREAM_BIDIRECTIONAL:
			case SGPIO_MODE_STREAM_DATA_IN:
				pr_debug("sgpio: capturing data from the function's input buffer\n");
				sgpio_capture_remaining_data_for_function(sgpio, function);
				break;

			// For all other modes, we don't need to do anything.
			default:
				break;

		}
	}
}
