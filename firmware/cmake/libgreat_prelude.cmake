#
# This file is part of libgreat.
# Early configuration and helper defines for using libgreat.
#
# Should be included before the main project() call.
#

# If we don't have a platform configuration, this can't be right.
if (NOT LIBGREAT_PLATFORM)
	message(FATAL_ERROR "libgreat firmware cannot be built without a configured platform! Check the including code.")
endif()

if (NOT CMAKE_TOOLCHAIN_FILE)
    message(FATAL_ERROR "libgreat firmware cannot be built without a cross-compile toolchain! Check the including code.")
endif()

# Libgreat paths.
# TODO: move most of these to a libgreat.cmake?
set(PATH_LIBGREAT_FIRMWARE                  ${PATH_LIBGREAT}/firmware)
set(PATH_LIBGREAT_FIRMWARE_CMAKE            ${PATH_LIBGREAT_FIRMWARE}/cmake)
set(PATH_LIBGREAT_FIRMWARE_DRIVERS          ${PATH_LIBGREAT_FIRMWARE}/drivers)
set(PATH_LIBGREAT_FIRMWARE_PLATFORM         ${PATH_LIBGREAT_FIRMWARE}/platform/${LIBGREAT_PLATFORM})
set(PATH_LIBGREAT_FIRMWARE_PLATFORM_DRIVERS ${PATH_LIBGREAT_FIRMWARE_PLATFORM}/drivers)

# CMake compatibility for older CMake versions.
include(${PATH_LIBGREAT_FIRMWARE_CMAKE}/compatibility.cmake)
