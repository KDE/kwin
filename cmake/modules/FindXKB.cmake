#.rst:
# FindXKB
# -------
#
# Try to find xkbcommon on a Unix system
# If found, this will define the following variables:
#
#   ``XKB_FOUND``
#     True if XKB is available
#   ``XKB_LIBRARIES``
#     Link these to use XKB
#   ``XKB_INCLUDE_DIRS``
#     Include directory for XKB
#   ``XKB_DEFINITIONS``
#     Compiler flags for using XKB
#
# Additionally, the following imported targets will be defined:
#
#   ``XKB::XKB``
#     The XKB library

#=============================================================================
# SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
#
# SPDX-License-Identifier: BSD-3-Clause
#=============================================================================

if(CMAKE_VERSION VERSION_LESS 2.8.12)
    message(FATAL_ERROR "CMake 2.8.12 is required by FindXKB.cmake")
endif()
if(CMAKE_MINIMUM_REQUIRED_VERSION VERSION_LESS 2.8.12)
    message(AUTHOR_WARNING "Your project should require at least CMake 2.8.12 to use FindXKB.cmake")
endif()

if(NOT WIN32)
    # Use pkg-config to get the directories and then use these values
    # in the FIND_PATH() and FIND_LIBRARY() calls
    find_package(PkgConfig)
    pkg_check_modules(PKG_XKB QUIET xkbcommon)

    set(XKB_DEFINITIONS ${PKG_XKB_CFLAGS_OTHER})

    find_path(XKB_INCLUDE_DIR
        NAMES
            xkbcommon/xkbcommon.h
        HINTS
            ${PKG_XKB_INCLUDE_DIRS}
    )
    find_library(XKB_LIBRARY
        NAMES
            xkbcommon
        HINTS
            ${PKG_XKB_LIBRARY_DIRS}
    )

    set(XKB_LIBRARIES ${XKB_LIBRARY})
    set(XKB_INCLUDE_DIRS ${XKB_INCLUDE_DIR})
    set(XKB_VERSION ${PKG_XKB_VERSION})

    include(FindPackageHandleStandardArgs)
    find_package_handle_standard_args(XKB
        FOUND_VAR
            XKB_FOUND
        REQUIRED_VARS
            XKB_LIBRARY
            XKB_INCLUDE_DIR
        VERSION_VAR
            XKB_VERSION
    )

    if(XKB_FOUND AND NOT TARGET XKB::XKB)
        add_library(XKB::XKB UNKNOWN IMPORTED)
        set_target_properties(XKB::XKB PROPERTIES
            IMPORTED_LOCATION "${XKB_LIBRARY}"
            INTERFACE_COMPILE_OPTIONS "${XKB_DEFINITIONS}"
            INTERFACE_INCLUDE_DIRECTORIES "${XKB_INCLUDE_DIR}"
        )
    endif()

else()
    message(STATUS "FindXKB.cmake cannot find XKB on Windows systems.")
    set(XKB_FOUND FALSE)
endif()

include(FeatureSummary)
set_package_properties(XKB PROPERTIES
    URL "https://xkbcommon.org"
    DESCRIPTION "XKB API common to servers and clients"
)
