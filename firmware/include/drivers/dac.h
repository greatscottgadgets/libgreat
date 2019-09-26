/*
 * This file is part of libgreat
 *
 * Generic DAC driver header.
 */

#ifndef __LIBGREAT_DAC_H__
#define __LIBGREAT_DAC_H__

#include <toolchain.h>
#include <drivers/platform_dac.h>

typedef struct dac {
	platform_dac_registers_t *reg;
} dac_t;


int dac_init(dac_t *dac);

#endif
