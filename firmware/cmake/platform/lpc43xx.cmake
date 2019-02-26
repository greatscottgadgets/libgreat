#
# This file is part of greatfet.
# Board support for LPC43xx-based boards.
#
include_guard()

# Define that this is an LPC43xx platform.
set(LIBGREAT_PLATFORM  lpc43xx)

# TODO: do we want to use the hard-float ABI, or do we want to make these inter-linkable?
set(FLAGS_MAIN_CPU                 -mcpu=cortex-m4 -mfloat-abi=hard -mfpu=fpv4-sp-d16)
set(FLAGS_SECONDARY_CPU            -mcpu=cortex-m0 -mfloat-abi=soft)

set(FLAGS_LINK_BOARD               -nostartfiles -Wl,--gc-sections)
set(FLAGS_LINK_MAIN_CPU            -Xlinker -Map=m4.map)
set(FLAGS_LINK_SECONDARY_CPU       -Xlinker -Map=m0.map)

# FIXME: don't include libopencm3!
set(LINK_LIBRARIES_BOARD           opencm3_lpc43xx)
set(DEFINES_BOARD                  ${DEFINES_BOARD} LPC43XX_M4)

# TODO: make these just the linker scripts, and not the flags
set(LINKER_SCRIPT_FLASH            libgreat_lpc43xx_rom_to_ram.ld)
set(LINKER_SCRIPT_DFU              libgreat_lpc43xx.ld)
set(LINKER_SCRIPTS_SECONDARY_CPU   LPC43xx_M0_memory.ld libopencm3_lpc43xx_m0.ld)

# Use our Cortex-M toolchain.
set(CMAKE_TOOLCHAIN_FILE           "${PATH_LIBGREAT}/firmware/cmake/toolchain/arm-cortex-m.cmake")
mark_as_advanced(CMAKE_TOOLCHAIN_FILE)
