#
# This file is part of GreatFET
#
cmake_minimum_required(VERSION 3.1.3)

include(cmake/libgreat_prelude.cmake)
project(libgreat C CXX ASM)
include(cmake/libgreat.cmake)

# Include any platform-specific targets desired.
add_subdirectory(${PATH_LIBGREAT_FIRMWARE_PLATFORM})

# Include the core libgreat library.
add_libgreat_library_if_necessary(libgreat OBJECT

	# Archictecture support.
	# TODO: move me to an architecture support package?
	${PATH_LIBGREAT_FIRMWARE_PLATFORM}/sync.c
	${PATH_LIBGREAT_FIRMWARE_PLATFORM}/sync.S

	${PATH_LIBGREAT_FIRMWARE_PLATFORM_DRIVERS}/arm_system_control.c
)
use_libgreat_modules(libgreat bsp) # Always include the board support package for the current board.

# Provide a basic allocator.
define_libgreat_module(allocator
	${PATH_LIBGREAT_FIRMWARE_DRIVERS}/memory/allocator.c
	${PATH_LIBGREAT_FIRMWARE_DRIVERS}/memory/allocator/umm_malloc.c
)

# Provide the core communications protocol.
define_libgreat_module(comms
	${PATH_LIBGREAT_FIRMWARE_DRIVERS}/comms/utils.c
	${PATH_LIBGREAT_FIRMWARE_DRIVERS}/comms/comms_class.c
	${PATH_LIBGREAT_FIRMWARE}/classes/core.c
	${PATH_LIBGREAT_FIRMWARE}/classes/firmware.c
)

# DAC drivers.
define_libgreat_module(ad970x
	${PATH_LIBGREAT_FIRMWARE_DRIVERS}/dac/ad970x.c
)

# Scheduler.
define_libgreat_module(scheduler
	${PATH_LIBGREAT_FIRMWARE_DRIVERS}/scheduler.c
)

# FIXME: get rid of this
add_dependencies(libgreat libopencm3)
