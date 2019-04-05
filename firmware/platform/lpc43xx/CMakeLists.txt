#
# This file is part of GreatFET
#
cmake_minimum_required(VERSION 3.1.3)

include(../../cmake/libgreat_prelude.cmake)
project(libgreat_platform_lpc43xx C ASM)
include(../../cmake/libgreat.cmake)

#
# FIXME: fix the path structure -- these all should be relative to the current source dir
# and should be simply paths like -- /drivers/usb/usb.c. The restructure for this is already done
# in other branches, but given how we've chosen to rebase we won't get this for a few more commits.
#

# Core board support.
define_libgreat_module(bsp

	# Start of day code.
	${PATH_LIBGREAT_FIRMWARE_PLATFORM}/crt0.c

	# Timers.
	${PATH_LIBGREAT_FIRMWARE_DRIVERS}/timer.c
	${PATH_LIBGREAT_FIRMWARE_PLATFORM_DRIVERS}/platform_timer.c

	# Platform configuration.
	${PATH_LIBGREAT_FIRMWARE_PLATFORM_DRIVERS}/platform_config.c

	# Reset control.
	${PATH_LIBGREAT_FIRMWARE_DRIVERS}/reset.c
	${PATH_LIBGREAT_FIRMWARE_PLATFORM_DRIVERS}/platform_reset.c

	# Clock control / generation.
	${PATH_LIBGREAT_FIRMWARE_PLATFORM_DRIVERS}/platform_clock.c
)


# Provide a USB driver stack.
define_libgreat_module(usb
	${PATH_LIBGREAT_FIRMWARE_PLATFORM_DRIVERS}/usb/usb.c
	${PATH_LIBGREAT_FIRMWARE_PLATFORM_DRIVERS}/usb/usb_host.c
	${PATH_LIBGREAT_FIRMWARE_PLATFORM_DRIVERS}/usb/usb_request.c
	${PATH_LIBGREAT_FIRMWARE_PLATFORM_DRIVERS}/usb/usb_standard_request.c
	${PATH_LIBGREAT_FIRMWARE_PLATFORM_DRIVERS}/usb/usb_queue.c
	${PATH_LIBGREAT_FIRMWARE_PLATFORM_DRIVERS}/usb/usb_queue_host.c
)

# Allow modules to communicate via the comms protocol and USB.
# TODO: automatically handle dependency management, here?
define_libgreat_module(usb_comms
	${PATH_LIBGREAT_FIRMWARE_PLATFORM_DRIVERS}/usb/comms_backend.c
)

# GPIO module.
# TODO: move to a platform module collection?
define_libgreat_module(gpio
	${PATH_LIBGREAT_FIRMWARE_PLATFORM_DRIVERS}/gpio.c
)

# Backtrace support.
define_libgreat_module(debug-backtrace ${PATH_LIBGREAT_FIRMWARE}/third-party/backtrace/backtrace.c)
libgreat_module_include_directories(debug-backtrace ${PATH_LIBGREAT_FIRMWARE}/third-party/backtrace/)
