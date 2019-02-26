#
# This file is part of greatfet.
# Common board configuration for LPC4330-based boards.
#
include_guard()

# Derive our configuration from the LPC43xx platform code.
include(${PATH_LIBGREAT_FIRMWARE_CMAKE}/platform/lpc43xx.cmake)

# Specify the individual part number we're using.
set(LIBGREAT_PART lpc4330)

# Add on the LPC4330 linker script for the main CPU.
set(LINKER_SCRIPTS_MAIN_CPU  ${LINKER_SCRIPTS_MAIN_CPU} LPC4330_M4_memory.ld)
