#
# This file is part of GreatFET.
# Legacy support stub for libopencm3. It will be removed as soon as we can. :)
#
include_guard()

include(ExternalProject)

# XXX Don't require libopencm3 from another place. Ick!
# FIXME: Don't require libopencm3 at all. >.>
set(PATH_LIBOPENCM3 ${PATH_GREATFET_FIRMWARE}/libopencm3)

# Specify how we build libopencm3.
if (NOT TARGET libopencm3)
	ExternalProject_Add(libopencm3
		SOURCE_DIR "${PATH_LIBOPENCM3}"
		BUILD_IN_SOURCE true
		DOWNLOAD_COMMAND ""
		CONFIGURE_COMMAND ""
		INSTALL_COMMAND ""
	)
endif()
