#
# This file is part of GreatFET
#
# Support for loading a secondary program to the m0 coprocessor.
#


set(SOURCE_M0 ${PATH_GREATFET_FIRMWARE_COMMON}/m0_sleep.c)

configure_file(${PATH_GREATFET_FIRMWARE}/cmake/m0_bin.s.cmake m0_bin.s)

add_executable(greatfet_usb_m0.elf ${SOURCE_M0})

target_compile_options(greatfet_usb_m0.elf PRIVATE ${FLAGS_COMPILE_COMMON} ${FLAGS_CPU_COMMON} ${FLAGS_CPU_M0})
target_compile_definitions(greatfet_usb_m0.elf PRIVATE ${DEFINES_COMMON})
target_link_options(greatfet_usb_m0.elf PRIVATE ${FLAGS_CPU_COMMON} ${FLAGS_CPU_M0} ${FLAGS_LINK_COMMON} ${FLAGS_LINK_M0})

target_link_directories(greatfet_usb_m0.elf PRIVATE ${PATH_LIBOPENCM3}/lib ${PATH_LIBOPENCM3}/lib/lpc43xx ${PATH_LIBGREAT}/firmware/platform/lpc43xx/linker)

target_link_libraries(greatfet_usb_m0.elf PRIVATE c nosys opencm3_lpc43xx_m0)

# ELF -> bin
add_custom_target(greatfet_usb_m0.bin ALL DEPENDS greatfet_usb_m0.elf COMMAND ${CMAKE_OBJCOPY} -Obinary greatfet_usb_m0.elf greatfet_usb_m0.bin)

# this should add a depdency to the parent target
