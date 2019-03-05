/*
 * This file is part of libgreat
 *
 * LPC43xx misc configuration register control
 */

#include <drivers/platform_config.h>

/**
 * Return a reference to the LPC43xx's CREG block.
 */
platform_configuration_registers_t *get_platform_configuration_registers(void)
{
	return (platform_configuration_registers_t *)0x40043000;
}


/**
 * Remaps the M4 core's address zero to exist in the given region.
 *
 * @param base_addr A pointer to the region to be mapped in.
 */
void platform_remap_address_zero(volatile void *base_addr)
{
	get_platform_configuration_registers()->m4memmap = (uint32_t)base_addr;
}


/**
 * @return returns true iff the calling thread is running on the M4
 */
bool platform_running_on_m4(void)
{
#ifdef LPC43XX_M4
	return true;
#else
	return false;
#endif
}


/**
 * @return returns true iff the calling thread is running on the M0
 */
bool platform_running_on_m0(void)
{
#ifdef LPC43XX_M0
	return true;
#else
	return false;
#endif
}
