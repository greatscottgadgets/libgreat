/*
 * This file is part of libgreat
 *
 * General Ethernet Complex drivers
 */

#include <drivers/ethernet.h>


/**
 * Initialies a new ethernet controller object, and readies it (and the appropriate)
 * hardware for use.
 *
 * @param An unpopulated ethernet device structure to be readied for use.
 */
void ethernet_init(ethernet_controller_t *device)
{
	// Perform the core low-level initialization for the ethernet controller.
	platform_ethernet_init(device);
}
