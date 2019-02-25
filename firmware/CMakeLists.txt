#
# This file is part of GreatFET
#

cmake_minimum_required(VERSION 3.1.3)

#set(CMAKE_TOOLCHAIN_FILE ../firmware/cmake/toolchain-arm-cortex-m.cmake)

project(libgreat C CXX ASM)

# Horrible hack: use libopencm3, for now.
include ("${CMAKE_CURRENT_SOURCE_DIR}/cmake/libopencm3.cmake")

set(LD_SCRIPT "-T${CMAKE_CURRENT_SOURCE_DIR}/../firmware/common/LPC43xx_M4_memory.ld -Tlibopencm3_lpc43xx_rom_to_ram.ld -T${CMAKE_CURRENT_SOURCE_DIR}/..firmware/common/LPC43xx_M4_M0_image_from_text.ld")
set(LD_SCRIPT_DFU "-T${CMAKE_CURRENT_SOURCE_DIR}/../firmware/common/LPC4330_M4.memory.ld -Tlibopencm3_lpc43xx.ld -T${CMAKE_CURRENT_SOURCE_DIR}/../firmware/common/LPC43xx_M4_M0_image_from_text.ld")

file(GLOB_RECURSE CLASSES_LIBGREAT_COMMON_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/classes/*.c")
message(STATUS "Using classes: " ${CLASSES_LIBGREAT_COMMON_SOURCES})

add_library(libgreat OBJECT
	${CMAKE_CURRENT_SOURCE_DIR}/platform/lpc43xx/crt0.c
	${CMAKE_CURRENT_SOURCE_DIR}/platform/lpc43xx/sync.c
	${CMAKE_CURRENT_SOURCE_DIR}/platform/lpc43xx/sync.S
	${CMAKE_CURRENT_SOURCE_DIR}/drivers/memory/allocator.c
	${CMAKE_CURRENT_SOURCE_DIR}/drivers/memory/allocator/umm_malloc.c
	${CMAKE_CURRENT_SOURCE_DIR}/drivers/usb/lpc43xx/usb.c
	${CMAKE_CURRENT_SOURCE_DIR}/drivers/usb/lpc43xx/usb_host.c
	${CMAKE_CURRENT_SOURCE_DIR}/drivers/usb/lpc43xx/usb_request.c
	${CMAKE_CURRENT_SOURCE_DIR}/drivers/usb/lpc43xx/usb_standard_request.c
	${CMAKE_CURRENT_SOURCE_DIR}/drivers/usb/lpc43xx/usb_queue.c
	${CMAKE_CURRENT_SOURCE_DIR}/drivers/usb/lpc43xx/usb_queue_host.c
	${CMAKE_CURRENT_SOURCE_DIR}/drivers/usb/comms_backend.c
	${CMAKE_CURRENT_SOURCE_DIR}/drivers/gpio/lpc43xx/gpio.c
	${CMAKE_CURRENT_SOURCE_DIR}/drivers/comms/comms_class.c
	${CMAKE_CURRENT_SOURCE_DIR}/drivers/comms/utils.c
	${CMAKE_CURRENT_SOURCE_DIR}/classes/core.c
	${CMAKE_CURRENT_SOURCE_DIR}/classes/firmware.c
	${CLASSES_LIBGREAT_COMMON_SOURCES}
	)

target_compile_definitions(libgreat PUBLIC ${DEFINES_COMMON} ${DEFINES_BOARD})

# FIXME: get rid of this!
add_dependencies(libgreat libopencm3)

# XXX fix this
target_include_directories(libgreat SYSTEM PRIVATE ${PATH_LIBOPENCM3}/include)
target_include_directories(libgreat PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/include
	${CMAKE_CURRENT_SOURCE_DIR}/include/platform/lpc43xx

	# XXX get rid of this
	${CMAKE_CURRENT_SOURCE_DIR}/../../firmware/common
	${BOARD_INCLUDE_DIRECTORIES}
	${BUILD_INCLUDE_DIRECTORIES}
)

target_compile_options(libgreat PRIVATE ${FLAGS_COMPILE_COMMON} ${FLAGS_BOARD} ${FLAGS_MAIN_CPU})
