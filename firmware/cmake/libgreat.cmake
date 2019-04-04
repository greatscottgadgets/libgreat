#
# This file is part of libgreat.
# Basic configuration and helper defines for using libgreat.
#
include_guard()

# Horrible hack: use libopencm3, for now.
include ("${PATH_LIBGREAT_FIRMWARE}/cmake/libopencm3.cmake")

#
# Function that prepends a given prefix to each menber of a list.
#
function(generate_linker_script_arguments OUTPUT_VARIABLE)
	set(ARGUMENTS "")

	# Append a -T flag for each linker script passed in.
	foreach (SCRIPT ${ARGN})
		list(APPEND ARGUMENTS "-T${SCRIPT}")
	endforeach(SCRIPT)

	# Set the relevant output variable.
	set("${OUTPUT_VARIABLE}" "${ARGUMENTS}" PARENT_SCOPE)
endfunction()


#
# Function that adds a flash "executable" target to the currrent build.
# Arguments: [binary_name] [sources...]
#
function(add_flash_executable EXECUTABLE_NAME)

	# Add a target for the ELF form of the executable.
	add_executable(${EXECUTABLE_NAME}.elf ${ARGN})

	# ... and set its default properties.
	generate_linker_script_arguments(FLAGS_LINKER_SCRIPTS ${LINKER_SCRIPTS_MAIN_CPU} ${LINKER_SCRIPT_FLASH})
	target_link_options(${EXECUTABLE_NAME}.elf PRIVATE
		${FLAGS_PLATFORM}
		${FLAGS_ARCHITECTURE}
		${FLAGS_MAIN_CPU}
		${FLAGS_LINK_BOARD}
		${FLAGS_LINKER_SCRIPTS}
		${FLAGS_LINK_MAIN_CPU}
	)
	target_link_directories(${EXECUTABLE_NAME}.elf PRIVATE
		${PATH_LIBOPENCM3}/lib
		${PATH_LIBOPENCM3}/lib/lpc43xx
		${PATH_LIBGREAT}/firmware/platform/${LIBGREAT_PLATFORM}/linker
	)

	target_link_libraries(${EXECUTABLE_NAME}.elf PRIVATE c nosys ${LINK_LIBRARIES_BOARD} m)

	# Add a target that creates the final binary.
	add_custom_target(${EXECUTABLE_NAME} ALL DEPENDS ${EXECUTABLE_NAME}.elf COMMAND ${CMAKE_OBJCOPY} -Obinary ${EXECUTABLE_NAME}.elf ${EXECUTABLE_NAME})

endfunction(add_flash_executable)

#
# Function that creates a new libgreat library / source collection.
# Arguments: <library_name> [sources...]
#
function(add_libgreat_library LIBRARY_NAME)

	# Create the relevant library.
	add_library(${LIBRARY_NAME} OBJECT ${ARGN})

	# And set its default properties.
	target_include_directories(${LIBRARY_NAME} PRIVATE
		${BOARD_INCLUDE_DIRECTORIES}
		${BUILD_INCLUDE_DIRECTORIES}
		${PATH_LIBOPENCM3}/include
		${PATH_GREATFET_FIRMWARE_COMMON} #XXX: remove this!
		${PATH_LIBGREAT_FIRMWARE}/include
		${PATH_LIBGREAT_FIRMWARE_PLATFORM}/include
	)
	target_compile_options(${LIBRARY_NAME} PRIVATE ${FLAGS_COMPILE_COMMON} ${FLAGS_PLATFORM} ${FLAGS_ARCHITECTURE} ${FLAGS_MAIN_CPU})
	target_compile_definitions(${LIBRARY_NAME} PRIVATE ${DEFINES_COMMON} ${DEFINES_BOARD})

endfunction(add_libgreat_library)

#
# Function that creates a new GreatFET library / source archive iff the relevant library does not exist.
# Arguments: [library_name] [sources...]
#
function(add_libgreat_library_if_necessary LIBRARY_NAME)

	# If the target doesn't already exist, create it.
	if (NOT TARGET ${LIBRARY_NAME})
		add_libgreat_library(${LIBRARY_NAME} ${ARGN})
	endif()

endfunction(add_libgreat_library_if_necessary)



#
# Function that provides a configurable libgreat feature, which can be optionally included in a given consume.
# Arguments: <module_name> [sources...]
#
function(define_libgreat_module MODULE_NAME)
	add_libgreat_library_if_necessary(libgreat_module_${MODULE_NAME} ${ARGN})

	# FIXME: don't have everything depend on libopencm3
	add_dependencies(libgreat_module_${MODULE_NAME} libopencm3)

endfunction(define_libgreat_module)


#
# Provides the include paths necessary to use a given libgreat module. Will automatically be added to the include path
# for the relevant libgreat module.
#
# Arguments: <module_name> [include_directories...]
#
function(libgreat_module_include_directories MODULE_NAME)

	# Adds the relevant include direcotires to the given libgreat module. These will also be added to the include path
	# of anything that uses the relevant module.
	target_include_directories(libgreat_module_${MODULE_NAME} PUBLIC ${ARGN})

endfunction(libgreat_module_include_directories)



#
# Function that adds a libgreat module to a given target.
# Arguments: <target_to_add_to> <module_to_add> [additional_modules...]
#
function(use_libgreat_modules TARGET_NAME)

	# Iterate over each of the provided modules.
	foreach (MODULE ${ARGN})

		# Compute the module's internal object name.
		set (MODULE_OBJECT libgreat_module_${MODULE})

		# Ensure that the relevant module exists.
		if (NOT TARGET libgreat_module_${MODULE})
			message(FATAL_ERROR "Cannot find the required libgreat module '${MODULE}'-- there's likely something wrong"
				" with the build configuration; or this module isn't supported on your platform.")
		endif()

		# Ensure that the relevant target depends on the given module...
		add_dependencies(${TARGET_NAME} libgreat_module_${MODULE})

		# ... and include the module's sources in the target.
		get_target_property(MODULE_SOURCES libgreat_module_${MODULE} SOURCES)
		target_sources(${TARGET_NAME} PRIVATE ${MODULE_SOURCES})

		# Include any includes specified for interfacing with the given target in the downstream target.
		get_target_property(ADDITIONAL_INCLUDES libgreat_module_${MODULE} INTERFACE_INCLUDE_DIRECTORIES)
		if (NOT ${ADDITIONAL_INCLUDES} MATCHES "-NOTFOUND$")
			target_include_directories(${TARGET_NAME} PRIVATE ${ADDITIONAL_INCLUDES})
		endif()

	endforeach(MODULE)

endfunction(use_libgreat_modules)
