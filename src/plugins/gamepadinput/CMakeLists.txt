function(ecm_add_qtwayland_server_protocol_kde target)
    # Parse arguments
    set(oneValueArgs PROTOCOL BASENAME PREFIX)
    cmake_parse_arguments(ARGS "" "${oneValueArgs}" "" ${ARGN})

    if(ARGS_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "Unknown keywords given to ecm_add_qtwayland_server_protocol_kde(): \"${ARGS_UNPARSED_ARGUMENTS}\"")
    endif()

    set(_prefix "${ARGS_PREFIX}")


    find_package(WaylandScanner REQUIRED QUIET)
    ecm_add_wayland_server_protocol(${target}
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

    target_sources(${target} PRIVATE "${_code}")
endfunction()

kcoreaddons_add_plugin(gamepadinput INSTALL_NAMESPACE "kwin/plugins")
target_sources(gamepadinput PRIVATE
    main.cpp
    gamepadinput.cpp
    gaminginput_v2_interface.cpp
    evdevcontroller.cpp
)
ecm_add_qtwayland_server_protocol_kde(gamepadinput
    PROTOCOL gaming-input-unstable-v2.xml
    BASENAME gaming-input-unstable-v2
)

ecm_qt_declare_logging_category(gamepadinput
    HEADER kwingamepadinput_logging.h
    IDENTIFIER KWIN_GAMEPAD
    CATEGORY_NAME kwin_gamepadinput
    DEFAULT_SEVERITY Warning
)

target_link_libraries(gamepadinput kwin KF6::Solid PkgConfig::Evdev)
if (BUILD_TESTING)
    add_subdirectory(tests)
endif()
