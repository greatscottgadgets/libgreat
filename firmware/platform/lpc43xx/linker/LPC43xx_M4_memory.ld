/*
 * This file is part of GreatFET
 */

MEMORY
{
	/* Physical address in Flash used to copy Code from Flash to RAM */
	/* ram_usb: USB buffer. Straddles two blocks of RAM
	 * to get performance benefit of having two USB buffers addressable
	 * simultaneously (on two different buses of the AHB multilayer matrix)
	 */
}

