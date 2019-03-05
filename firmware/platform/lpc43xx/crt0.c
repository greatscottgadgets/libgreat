/*
 * This file is part of libgreat.
 *
 * C Runtime 0: start of day code for the LPC4330
 */

// FIXME: get rid of all of these
#include <toolchain.h>

#include <drivers/platform_clock.h>
#include <drivers/platform_config.h>

#include <drivers/arm_system_control.h>

/* This special variable is preserved across soft resets by a little bit of
 * reset handler magic. It allows us to pass a Reason across resets. */
/* FIXME: use sections to do this instead of the below */
/* FIXME: make this static and provide an accessor, somewhere? */
/* FIXME: move this out of the crt0 and to its own driver? */
volatile uint32_t reset_reason ATTR_PERSISTENT;

/**
 * Define the main function, which allows us to refer to the user program.
 */
int main(void);

/**
 * Section start and end markers for the constructor and
 * destructor linker sections. Provided by the linker.
 */
typedef void (*funcp_t) (void);
extern funcp_t __preinit_array_start, __preinit_array_end;
extern funcp_t __init_array_start, __init_array_end;
extern funcp_t __fini_array_start, __fini_array_end;

/**
 * Section start and end markers for the standard program sections.
 * Provided by the linker.
 */
extern unsigned _data_loadaddr, _data, _edata, _bss, _ebss, _stack;
extern unsigned _text_segment_ram, _text_segment_rom;
extern unsigned _text_segment_end, _text_segment_rom_end, _text_segment_ram_end;


/**
 * Function to be called before main, but after an initializers.
 */
static void relocate_to_ram(void)
{
	volatile unsigned *load_source, *load_destination;
	volatile unsigned *load_target = &_text_segment_ram;

	/* If we need to relocate, relocate. */
	if (&_text_segment_ram != &_text_segment_rom) {

		// Figure out the location that we're relocating from.
		load_source = &_text_segment_rom_end - (&_text_segment_ram_end - load_target);

		/* Change Shadow memory to ROM (for Debug Purpose in case Boot
		 * has not set correctly the M4MEMMAP because of debug)
		 */
		platform_remap_address_zero(load_source);

		// FIXME: make this a memcpy
		for (load_destination = load_target; load_destination < &_text_segment_ram_end;) {
			*load_destination = *load_source;

			load_source++;
			load_destination++;
		}

		/* Change Shadow memory to Real RAM */
		platform_remap_address_zero(&_text_segment_ram);

		/* Continue Execution in RAM */
	}
}

extern unsigned int debug_read_index;
extern unsigned int debug_write_index;


/**
 * Prepare the system's CPU for use.
 */
void set_up_cpu(void)
{
	// Enable access to the system's FPUs.
	arch_enable_fpu(true);

	// Enable the early clocks necessary for basic functionality.
	platform_initialize_early_clocks();
}


/**
 * Startup code for the processor and general initialization.
 */
void ATTR_NORETURN reset_handler(void)
{
	volatile unsigned *src, *dest;
	funcp_t *fp;

	// Initialize the systems's data segment.
	for (src = &_data_loadaddr, dest = &_data; dest < &_edata; src++, dest++) {
		*dest = *src;
	}

	// Clear the system's BSS.
	for (dest = &_bss; dest < &_ebss; ) {
		*dest++ = 0;
	}

	// Configure the CPU into its full running state.
	set_up_cpu();

	// Begin executing the program from RAM, instead of
	// ROM, if desired. This improvides performance, as we
	// don't have to keep fetching over SPIFI.
	relocate_to_ram();

	// Initilize the bare-bones early clocks.
	platform_initialize_early_clocks();

	// Extremely early pre-init. This section is for initializer that
	// should run very early -- before we've even fully brought up the CPU clocking scheme.
	// This is for the very basics of getting our platform up and running.
	for (fp = &__preinit_array_start; fp < &__preinit_array_end; fp++) {
		(*fp)();
	}

	// With the pre-init complete, we're ready to begin platform initialization.
	// First, we'll perform the few steps that pivot us from early startup to
	// fully capable of using the hardware.
	platform_initialize_clocks();

	// Run each of the initializers.
	for (fp = &__init_array_start; fp < &__init_array_end; fp++) {
		(*fp)();
	}

	// Call the application's entry point.
	main();

	// Run any destructors on the platform.
	for (fp = &__fini_array_start; fp < &__fini_array_end; fp++) {
		(*fp)();
	}

	// TODO: trigger a system reset.
	while (1);
}
