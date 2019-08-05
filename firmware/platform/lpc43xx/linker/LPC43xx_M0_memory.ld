/*
 * This file is part of GreatFET
 */

MEMORY
{
	shadow     (rwx) : ORIGIN = 0x00000000, LENGTH = 8K
	ram        (rwx) : ORIGIN = 0x10090000, LENGTH = 8K
}
