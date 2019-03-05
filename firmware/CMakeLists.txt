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
	${CMAKE_CURRENT_SOURCE_DIR}/drivers/comms/utils.c
	${CMAKE_CURRENT_SOURCE_DIR}/drivers/comms/comms_class.c
	${CMAKE_CURRENT_SOURCE_DIR}/classes/core.c
	${CMAKE_CURRENT_SOURCE_DIR}/classes/firmware.c
)

# FIXME: get rid of this
add_dependencies(libgreat libopencm3)
