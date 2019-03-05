/*
 * This file is part of libgreat
 *
 * Generic Ethernet drivers
 */

#ifndef __LIBGREAT_ETHERNET_H__
#define __LIBGREAT_ETHERNET_H__

#include <toolchain.h>
#include <drivers/ethernet/platform.h>

/**
 * Data structure storing state for an ethernet controller.
 */
typedef struct ethernet_controller {

	// Reference to the (platform-specific) register block that controls the
	// system's ethernet controller.
	ethernet_register_block_t *reg;

	// Platform-specific data.
	ethernet_platform_data_t platform;


} ethernet_controller_t;


/**
 * Initialies a new ethernet controller, readying it for use.
 */
void ethernet_init(ethernet_controller_t *device);




#endif
