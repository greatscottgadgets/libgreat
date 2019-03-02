#
# This file is part of libgreat
#
# Extensions for adding an lpc43xx DFU target to a libgreat target.


# Adds a DFU executable that corresponds to a given
function(add_dfu_executable NEW_EXECUTABLE BASE_EXECUTABLE BASE_LINKER_SCRIPT DFU_LINKER_SCRIPT)

    # Read out the properties of the relevant target; so we can use them to create our own DFU version.
    get_target_property(SOURCES          ${BASE_EXECUTABLE}.elf SOURCES)
    get_target_property(LINK_OPTIONS     ${BASE_EXECUTABLE}.elf LINK_OPTIONS)
    get_target_property(LINK_DIRECTORIES ${BASE_EXECUTABLE}.elf LINK_DIRECTORIES)
    get_target_property(LINK_LIBRARIES   ${BASE_EXECUTABLE}.elf LINK_LIBRARIES)

    # Replace the base linker script with the target linker script.
    string(REPLACE "${BASE_LINKER_SCRIPT}" "${DFU_LINKER_SCRIPT}" DFU_LINK_OPTIONS "${LINK_OPTIONS}")

    # Create a new executable that will create the elf representation of the given target.
    add_executable(${NEW_EXECUTABLE}.elf ${SOURCES})
    set_target_properties(${NEW_EXECUTABLE}.elf PROPERTIES
        LINK_OPTIONS     "${DFU_LINK_OPTIONS}"
        LINK_DIRECTORIES "${LINK_DIRECTORIES}"
        LINK_LIBRARIES   "${LINK_LIBRARIES}"
    )

    # Add a custom target that converts the generated ELF file to a binary.
    add_custom_target(${NEW_EXECUTABLE}.bin ALL DEPENDS ${NEW_EXECUTABLE}.elf COMMAND ${CMAKE_OBJCOPY} -Obinary ${NEW_EXECUTABLE}.elf ${NEW_EXECUTABLE}.bin)

    # Add a custom target that converts the genrated binary to a DFU file.
    # FIXME: rewrite this to be a single command? (e.g. rewrite the python)
    add_custom_target(${NEW_EXECUTABLE} ${DFU_ALL} DEPENDS ${NEW_EXECUTABLE}.bin
        COMMAND rm -f _tmp.dfu _header.bin
        COMMAND cp ${NEW_EXECUTABLE}.bin _tmp.dfu
        COMMAND ${DFU_COMMAND}
        COMMAND python ${PATH_GREATFET_FIRMWARE}/dfu.py ${NEW_EXECUTABLE}.bin
        COMMAND cat _header.bin _tmp.dfu >${NEW_EXECUTABLE}
    )

endfunction(add_dfu_executable)

