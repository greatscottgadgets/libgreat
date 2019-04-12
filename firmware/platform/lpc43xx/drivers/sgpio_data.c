/*
 * SGPIO IRQ generation meta-code
 *
 * This file is part of libgreat.
 */


#include <toolchain.h>

#include <debug.h>
#include <errno.h>

#include <drivers/sgpio.h>
#include <drivers/arm_vectors.h>

// FIXME: this currently sets up a single ISR, but that's not appropriate for any setup where different slices
// can update at different rates. We should assume the template is _copied_ each time, so we can set up multiple
// handlers for multiple different events. This is still a TODO, but it needs to happen soon to maintain this driver's
// genericism.

#define pr_debug pr_info

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
 * @return the slice number that corresponds to the given position in the concatenated "slice" buffer
 */
static uint32_t sgpio_get_function_buffer_slice_index(sgpio_function_t *function, uint32_t position_in_buffer)
{
	uint32_t position_to_look_up = position_in_buffer;

	switch (function->mode) {

		// Pure-output modes.
		case SGPIO_MODE_STREAM_DATA_OUT:
		case SGPIO_MODE_FIXED_DATA_OUT:
			position_to_look_up = (position_in_buffer + 1) % function->bus_width;
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
 * Method that tries to avoid an ISR, whenever possible.
 */
bool sgpio_isr_necessary_for_function(sgpio_function_t *function)
{
	uint8_t slice_buffer_order_bytes         = function->buffer_depth_order + 2;
	uint8_t slice_buffer_order_with_exchange = slice_buffer_order_bytes + 1;

	switch (function->mode) {

		// If we're in clock generation, we can implicitly avoid an ISR.
		case SGPIO_MODE_CLOCK_GENERATION:
			return false;

		// If we're streaming fixed data out, we can avoid an ISR as long as we can fit the entire
		// pattern in our streaming buffer. That means as long as our user buffer's order (in bytes) is
		// less than the total order we have between both the shadow and data registers, we can just shift
		// directly from the slice buffers, and avoid an ISR to re-populate either.
		case SGPIO_MODE_FIXED_DATA_OUT:
			return (function->buffer_order > slice_buffer_order_with_exchange);

		// In all other cases, we need to generate an ISR.
		default:
			return true;
	}

}



/**
 * Method that generates assembly code for the SGPIO data-shuttling ISR. Essentially generates the code used
 * to copy data into and out of the SGPIO shadow buffers.
 */
int sgpio_generate_isr_for_function(sgpio_function_t *function)
{
	unsigned position_in_instruction_body = 0;
	unsigned position_in_user_buffer = 0;
	int rc;

	// Optimization: in some modes, we can avoid having an ISR completely.
	// If an ISR isn't necessary, we're trivially done!
	if (!sgpio_isr_necessary_for_function(function)) {
		return 0;
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
		return rc;
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
	return 0;
}


/**
 * Generates an ISR that handles shuttling around SGPIO data.
 */
interrupt_service_routine_t sgpio_generate_data_shuttle_isr(sgpio_t *sgpio)
{
	int rc;

	sgpio->swap_irqs_required = 0;

	// XXX: generate for every _applicable_ function and possibly generate a master ISR to stitch them all together?

	// Pretend we're conditionally generating ISRs.
	if (sgpio_isr_necessary_for_function(&sgpio->functions[0])) {
		sgpio->swap_irqs_required = 1 << sgpio->functions[0].io_slice;
	}

	// XXX Generate just function0's ISR.
	rc = sgpio_generate_isr_for_function(&sgpio->functions[0]);
	if (rc) {
		return NULL;
	}

	// XXX return the generated ISR, and not this hardcoded stub
	return sgpio_dynamic_isr;
}


/**
 * Prepopulates either the data or shadow registers associated with the given function with the next data from
 * the user registers, allowing
 */
static void sgpio_prepopulate_function_buffer(sgpio_function_t *function, volatile uint32_t *target_registers)
{
	// Get a quick reference to the user buffer, and compute its size in bytes and how far we are into it.
	uint32_t buffer_size_bytes = (1UL << function->buffer_order);

	// Populate the data buffer with the data to be shifted out immediately.
	uint8_t current_word_index  = (1UL << function->buffer_depth_order);
	while (current_word_index--) {

		// We wrap around the circuilar buffer -- this both allows us to "wrap around" the edge if our current position
		// starts at a location other than zero, and allows us to repeat the buffer multiple times if we have a buffer
		// shorter than the relevant concatinated slice chain.
		uintptr_t position_in_buffer    = (uintptr_t)(function->position_in_buffer % buffer_size_bytes);
		uint32_t *target_buffer_segment = (uint32_t *)((uintptr_t)function->buffer + position_in_buffer);

		// Copy a single slice at a time into the target register set.
		uint32_t slice_buffer_offset          = sgpio_get_function_buffer_slice_index(function, current_word_index);
		target_registers[slice_buffer_offset] = *target_buffer_segment;

		// And move to the next slice.
		function->position_in_buffer += sizeof(uint32_t);
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
			// For modes with output, we'll prepopulate the data and shadow registers.
			//
			case SGPIO_MODE_STREAM_DATA_OUT:
			case SGPIO_MODE_FIXED_DATA_OUT:
			case SGPIO_MODE_STREAM_BIDIRECTIONAL:
				sgpio_prepopulate_function_buffer(function, sgpio->reg->data);
				sgpio_prepopulate_function_buffer(function, sgpio->reg->data_shadow);
				break;

			// For all other modes, we don't need to do anything.
			default:
				break;

		}
	}
}
