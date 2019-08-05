/*
 * This file is part of GreatFET
 */

/* Linker script for GreatFET One (LPC4330, 2M SPI flash, 264K SRAM). */

MEMORY
{
	/* rom is really the shadow region that points to SPI flash or elsewhere */
	rom        (rx)  : ORIGIN = 0x00000000, LENGTH = 128K
	ram_local1 (rwx) : ORIGIN = 0x10000000, LENGTH = 128K
	ram_local2 (rwx) : ORIGIN = 0x10080000, LENGTH =  64K
	ram_m0     (rwx) : ORIGIN = 0x10090000, LENGTH =   8K
	ram_ahb    (rwx) : ORIGIN = 0x20000000, LENGTH =  32K
	ram_usb (rwx)    : ORIGIN = 0x20008000, LENGTH =  32K
	rom_flash (rx)   : ORIGIN = 0x80000000, LENGTH =   2M
}

usb_bulk_buffer = ORIGIN(ram_usb);
