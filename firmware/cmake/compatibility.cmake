#
# This file is part of libgreat.
# Compatibilty stub that enables us to work down to CMake 3.0.
#

# Compatibility with older CMake versions:

# Target link options are called using target_link_libraries in older versions of cmake.
if (NOT COMMAND target_link_options)
    function(target_link_options)
        target_link_libraries(${ARGN})
    endfunction(target_link_options)
endif()

# We can't use target_link_directories in older cmake; so we'll manually generate the flags for these directories.
if (NOT COMMAND target_link_directories)
    function(target_link_directories TARGET SCOPE)

        # Add each directory to the search path.
        foreach (DIRECTORY ${ARGN})
            target_link_options(${TARGET} ${SCOPE} "-L${DIRECTORY}")
        endforeach(DIRECTORY)

    endfunction(target_link_directories)
endif()

# include_guard() is too new; so we'll emulate it for older verseions of cmake.
if (NOT COMMAND include_guard)
    macro(include_guard)

        # Detemrine which scope we should use.
        if(${ARGN})
            if(${ARGN} STREQUAL "GLOBAL")
                set(SCOPE "GLOBAL")
            else()
                set(SCOPE "DIRECTORY")
            endif()
        else()
            set(SCOPE "VARIABLE")
        endif()

        # Check to see if a relevant variable has ever been included in this scope.
        set(__filename "${CMAKE_CURRENT_LIST_FILE}")
        get_property(already_included ${SCOPE} PROPERTY "pr_${__filename}")

        # If we have been included, abort.
        if(already_included)
            return()
        endif()

        # Set a properly-scoped variable that will indicate that this has already been included.
        if("${SCOPE}" STREQUAL "VARIABLE")
            set("pr_${__filename}" TRUE)
        else()
            set_property("${SCOPE}" PROPERTY "pr_${__filename}")
        endif()
    endmacro(include_guard)
endif()
