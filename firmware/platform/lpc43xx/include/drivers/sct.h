/*
 * LPC43xx State Configurable Timer
 *
 * This file is part of libgreat.
 */

#include <toolchain.h>

#ifndef __LIBGREAT_PLATFORM_SCT__
#define __LIBGREAT_PLATFORM_SCT__

typedef struct ATTR_PACKED {
	uint32_t state_mask;
	uint32_t control;
} event_registers_t;

typedef struct ATTR_PACKED {
	uint32_t set;
	uint32_t clear;
} output_registers_t;


/**
 * Registers that control the LPC43xx state controlled timer.
 */
typedef volatile struct ATTR_PACKED {

	uint32_t configuration;

	struct {
		uint32_t use_both_halves_as_one            : 1;
		uint32_t clock_mode                        : 2;
		uint32_t clock_input_number                : 3;
		uint32_t clock_on_falling_edges            : 1;
		uint32_t prevent_lower_half_from_reloading : 1;
		uint32_t prevent_upper_half_from_reloading : 1;
		uint32_t synchronize_input_0               : 1;
		uint32_t synchronize_input_1               : 1;
		uint32_t synchronize_input_2               : 1;
		uint32_t synchronize_input_3               : 1;
		uint32_t synchronize_input_4               : 1;
		uint32_t synchronize_input_5               : 1;
		uint32_t synchronize_input_6               : 1;
		uint32_t synchronize_input_7               : 1;
		uint32_t                                   : 15;
	};

	uint32_t control;
	uint32_t limit;
	uint32_t halt;
	uint32_t stop;
	uint32_t start;

	RESERVED_WORDS(10);

	uint32_t count;
	uint32_t state;

	uint32_t input;

	uint32_t register_modes;

	uint32_t raw_output;
	uint32_t output_direction;

	uint32_t conflict_resolution;

	uint32_t dma_request[2];

	RESERVED_WORDS(35);

	uint32_t event_enable;
	uint32_t event_flag;

	uint32_t conflict_enable;
	uint32_t conflict_flag;

	union {
		uint32_t match[16];
		uint32_t capture[16];
	};

	RESERVED_WORDS(16);

	union {
		uint32_t match_alias_low[16];
		uint32_t capture_alias_low[16];
	};
	union {
		uint32_t match_alias_high[16];
		uint32_t capture_alias_high[16];
	};


	union {
		uint32_t match_reload[16];
		uint32_t capture_control[16];
	};

	RESERVED_WORDS(16);

	uint32_t capture_control_alias_low[16];
	uint32_t capture_control_alias_high[16];

	event_registers_t event[16];

	RESERVED_WORDS(96);

	output_registers_t output[16];

} platform_sct_register_block_t;

ASSERT_OFFSET(platform_sct_register_block_t, count,                     0x040);
ASSERT_OFFSET(platform_sct_register_block_t, event_enable,              0x0f0);
ASSERT_OFFSET(platform_sct_register_block_t, match_alias_low,           0x180);
ASSERT_OFFSET(platform_sct_register_block_t, capture_control_alias_low, 0x280);
ASSERT_OFFSET(platform_sct_register_block_t, event,                     0x300);
ASSERT_OFFSET(platform_sct_register_block_t, output,                    0x500);


#endif
