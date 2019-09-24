/*
 * This file is part of libgreat
 *
 * LPC43xx DAC functions
 */

#include <toolchain.h>

#include <drivers/dac.h>
#include <drivers/platform_dac.h>

platform_dac_registers_t *platform_get_dac_registers()
{
	// DAC base address.
	return (platform_dac_registers_t *)0x400E1000;
}

int platform_dac_init(dac_t *dac)
{
	// Enable the DAC as well as DMA for it.
	dac->reg->dma_and_dac_enable = 1;

	return 0;
}

void dac_set_value(dac_t *dac, uint32_t value)
{
	dac->reg->conversion_value = value;
}
