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

# Provide a USB driver stack.
# FIXME: move these into a seperate cmake for the platform?
define_libgreat_module(usb
	${PATH_LIBGREAT_FIRMWARE}/drivers/usb/${LIBGREAT_PLATFORM}/usb.c
	${PATH_LIBGREAT_FIRMWARE}/drivers/usb/${LIBGREAT_PLATFORM}/usb_host.c
	${PATH_LIBGREAT_FIRMWARE}/drivers/usb/${LIBGREAT_PLATFORM}/usb_request.c
	${PATH_LIBGREAT_FIRMWARE}/drivers/usb/${LIBGREAT_PLATFORM}/usb_standard_request.c
	${PATH_LIBGREAT_FIRMWARE}/drivers/usb/${LIBGREAT_PLATFORM}/usb_queue.c
	${PATH_LIBGREAT_FIRMWARE}/drivers/usb/${LIBGREAT_PLATFORM}/usb_queue_host.c
)

# Allow modules to communicate via the comms protocol and USB.
# TODO: automatically handle dependency management, here?
define_libgreat_module(usb_comms
	${PATH_LIBGREAT_FIRMWARE}/drivers/usb/comms_backend.c
)

# GPIO module.
# TODO: move to a platform module collection?
define_libgreat_module(gpio
	${PATH_LIBGREAT_FIRMWARE}/drivers/gpio/lpc43xx/gpio.c
)
