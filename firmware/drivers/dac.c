/*
 * This file is part of libgreat
 *
 * Generic DAC driver.
 */

#include <toolchain.h>
#include <drivers/dac.h>
#include <drivers/platform_dac.h>

int dac_init(dac_t *dac)
{
	dac->reg = platform_get_dac_registers();

	int rc = platform_dac_init(dac);
	if (rc) {
		return rc;
	}

	return 0;
}
