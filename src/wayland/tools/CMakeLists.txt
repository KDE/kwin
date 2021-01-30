remove_definitions(-DQT_NO_CAST_FROM_ASCII)
add_executable(qtwaylandscanner_kde qtwaylandscanner.cpp)
target_link_libraries(qtwaylandscanner_kde Qt::Core)

function(ecm_add_qtwayland_server_protocol_kde out_var)
    # Parse arguments
    set(oneValueArgs PROTOCOL BASENAME PREFIX)
    cmake_parse_arguments(ARGS "" "${oneValueArgs}" "" ${ARGN})

    if(ARGS_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "Unknown keywords given to ecm_add_qtwayland_server_protocol_kde(): \"${ARGS_UNPARSED_ARGUMENTS}\"")
    endif()

    set(_prefix "${ARGS_PREFIX}")


    find_package(WaylandScanner REQUIRED QUIET)
    ecm_add_wayland_server_protocol(${out_var}
                                    PROTOCOL ${ARGS_PROTOCOL}
                                    BASENAME ${ARGS_BASENAME})

    get_filename_component(_infile ${ARGS_PROTOCOL} ABSOLUTE)
    set(_header "${CMAKE_CURRENT_BINARY_DIR}/qwayland-server-${ARGS_BASENAME}.h")
    set(_code "${CMAKE_CURRENT_BINARY_DIR}/qwayland-server-${ARGS_BASENAME}.cpp")

    set_source_files_properties(${_header} ${_code} GENERATED)

    add_custom_command(OUTPUT "${_header}"
        COMMAND qtwaylandscanner_kde server-header ${_infile} "" ${_prefix} > ${_header}
        DEPENDS ${_infile} qtwaylandscanner_kde VERBATIM)

    add_custom_command(OUTPUT "${_code}"
        COMMAND qtwaylandscanner_kde server-code ${_infile} "" ${_prefix} > ${_code}
        DEPENDS ${_infile} ${_header} qtwaylandscanner_kde VERBATIM)

    set_property(SOURCE ${_header} ${_code} PROPERTY SKIP_AUTOMOC ON)

    list(APPEND ${out_var} "${_code}")
    set(${out_var} ${${out_var}} PARENT_SCOPE)
endfunction()
