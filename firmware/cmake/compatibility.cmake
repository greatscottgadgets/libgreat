#
# This file is part of libgreat.
# Compatibilty stub that enables us to work down to CMake 3.0.
#

# Compatibility with older CMake versions.
if (NOT COMMAND target_link_options)
    function(target_link_options)
        target_link_libraries(${ARGN})
    endfunction()
endif()
