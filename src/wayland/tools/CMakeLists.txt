# Allow building just qtwaylandscanner_kde when cross-compiling since the
# dependencies for the rest of the targets might not be installed on the host.
if (${CMAKE_CURRENT_SOURCE_DIR} STREQUAL ${CMAKE_SOURCE_DIR})
    cmake_minimum_required(VERSION 3.16)
    project(qtwaylandscanner_kde CXX)
    set(CMAKE_CXX_STANDARD 20)
    find_package(Qt6 5.15.0 CONFIG REQUIRED Core)
    find_package(ECM ${KF6_MIN_VERSION} NO_MODULE REQUIRED)
    set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})
endif()

# when cross compiling, use either the executable offered or try to cross-compile it in place.
if(CMAKE_CROSSCOMPILING AND QTWAYLANDSCANNER_KDE_EXECUTABLE)
    add_executable(qtwaylandscanner_kde IMPORTED GLOBAL)
    set_target_properties(qtwaylandscanner_kde PROPERTIES IMPORTED_LOCATION ${QTWAYLANDSCANNER_KDE_EXECUTABLE})
elseif(CMAKE_CROSSCOMPILING)
    if (NOT KF6_HOST_TOOLING)
        message(FATAL_ERROR "Please provide a prefix with a native Qt build and pass -DKF6_HOST_TOOLING=path")
    endif()

    # search native tooling prefix
    set(NATIVE_PREFIX "" CACHE STRING "CMAKE_PREFIX_PATH for native Qt libraries")
    if (NOT NATIVE_PREFIX)
        string(FIND ${KF6_HOST_TOOLING} /lib idx)
        string(SUBSTRING ${KF6_HOST_TOOLING} 0 ${idx} NATIVE_PREFIX)
    endif()
    message(STATUS "Building qtwaylandscanner_kde against ${NATIVE_PREFIX}")

    include(ExternalProject)
    ExternalProject_Add(native_qtwaylandscanner_kde
            SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}
            CMAKE_ARGS -DECM_DIR=${ECM_DIR} -DCMAKE_PREFIX_PATH=${NATIVE_PREFIX}
            -DCMAKE_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}
            -DQT_MAJOR_VERSION=${QT_MAJOR_VERSION}
            INSTALL_COMMAND ""
            BUILD_BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/native_qtwaylandscanner_kde-prefix/src/native_qtwaylandscanner_kde-build/qtwaylandscanner_kde
            )
    add_executable(qtwaylandscanner_kde IMPORTED GLOBAL)
    add_dependencies(qtwaylandscanner_kde native_qtwaylandscanner_kde)
    set_target_properties(qtwaylandscanner_kde PROPERTIES IMPORTED_LOCATION
            ${CMAKE_CURRENT_BINARY_DIR}/native_qtwaylandscanner_kde-prefix/src/native_qtwaylandscanner_kde-build/qtwaylandscanner_kde)
else()
    # host build
    remove_definitions(-DQT_NO_CAST_FROM_ASCII)
    add_executable(qtwaylandscanner_kde qtwaylandscanner.cpp)
    target_link_libraries(qtwaylandscanner_kde Qt::Core)
    include(ECMMarkNonGuiExecutable)
    ecm_mark_nongui_executable(qtwaylandscanner_kde)
endif()

function(ecm_add_qtwayland_server_protocol_kde target)
    # Parse arguments
    set(options PRIVATE_CODE)
    set(oneValueArgs PROTOCOL BASENAME PREFIX)
    cmake_parse_arguments(ARGS "${options}" "${oneValueArgs}" "" ${ARGN})

    if(ARGS_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "Unknown keywords given to ecm_add_qtwayland_server_protocol_kde(): \"${ARGS_UNPARSED_ARGUMENTS}\"")
    endif()

    set(_prefix "${ARGS_PREFIX}")

    if(ARGS_PRIVATE_CODE)
        set(_private_code_option PRIVATE_CODE)
    endif()

    find_package(WaylandScanner REQUIRED QUIET)
    ecm_add_wayland_server_protocol(${target}
                                    PROTOCOL ${ARGS_PROTOCOL}
                                    BASENAME ${ARGS_BASENAME}
                                    ${_private_code_option})

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
