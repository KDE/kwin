#.rst:
# FindLibinput
# -------
#
# Try to find libinput on a Unix system.
#
# This will define the following variables:
#
# ``Libinput_FOUND``
#     True if (the requested version of) libinput is available
# ``Libinput_VERSION``
#     The version of libinput
# ``Libinput_LIBRARIES``
#     This can be passed to target_link_libraries() instead of the ``Libinput::Libinput``
#     target
# ``Libinput_INCLUDE_DIRS``
#     This should be passed to target_include_directories() if the target is not
#     used for linking
# ``Libinput_DEFINITIONS``
#     This should be passed to target_compile_options() if the target is not
#     used for linking
#
# If ``Libinput_FOUND`` is TRUE, it will also define the following imported target:
#
# ``Libinput::Libinput``
#     The libinput library
#
# In general we recommend using the imported target, as it is easier to use.
# Bear in mind, however, that if the target is in the link interface of an
# exported library, it must be made available by the package config file.

#=============================================================================
# SPDX-FileCopyrightText: 2014 Alex Merry <alex.merry@kde.org>
# SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
#
# SPDX-License-Identifier: BSD-3-Clause
#=============================================================================

if(CMAKE_VERSION VERSION_LESS 2.8.12)
    message(FATAL_ERROR "CMake 2.8.12 is required by FindLibinput.cmake")
endif()
if(CMAKE_MINIMUM_REQUIRED_VERSION VERSION_LESS 2.8.12)
    message(AUTHOR_WARNING "Your project should require at least CMake 2.8.12 to use FindLibinput.cmake")
endif()

if(NOT WIN32)
    # Use pkg-config to get the directories and then use these values
    # in the FIND_PATH() and FIND_LIBRARY() calls
    find_package(PkgConfig)
    pkg_check_modules(PKG_Libinput QUIET libinput)

    set(Libinput_DEFINITIONS ${PKG_Libinput_CFLAGS_OTHER})
    set(Libinput_VERSION ${PKG_Libinput_VERSION})

    find_path(Libinput_INCLUDE_DIR
        NAMES
            libinput.h
        HINTS
            ${PKG_Libinput_INCLUDE_DIRS}
    )
    find_library(Libinput_LIBRARY
        NAMES
            input
        HINTS
            ${PKG_Libinput_LIBRARY_DIRS}
    )

    include(FindPackageHandleStandardArgs)
    find_package_handle_standard_args(Libinput
        FOUND_VAR
            Libinput_FOUND
        REQUIRED_VARS
            Libinput_LIBRARY
            Libinput_INCLUDE_DIR
        VERSION_VAR
            Libinput_VERSION
    )

    if(Libinput_FOUND AND NOT TARGET Libinput::Libinput)
        add_library(Libinput::Libinput UNKNOWN IMPORTED)
        set_target_properties(Libinput::Libinput PROPERTIES
            IMPORTED_LOCATION "${Libinput_LIBRARY}"
            INTERFACE_COMPILE_OPTIONS "${Libinput_DEFINITIONS}"
            INTERFACE_INCLUDE_DIRECTORIES "${Libinput_INCLUDE_DIR}"
        )
    endif()

    mark_as_advanced(Libinput_LIBRARY Libinput_INCLUDE_DIR)

    # compatibility variables
    set(Libinput_LIBRARIES ${Libinput_LIBRARY})
    set(Libinput_INCLUDE_DIRS ${Libinput_INCLUDE_DIR})
    set(Libinput_VERSION_STRING ${Libinput_VERSION})

else()
    message(STATUS "FindLibinput.cmake cannot find libinput on Windows systems.")
    set(Libinput_FOUND FALSE)
endif()

include(FeatureSummary)
set_package_properties(Libinput PROPERTIES
    URL "https://www.freedesktop.org/wiki/Software/libinput/"
    DESCRIPTION "Library to handle input devices in Wayland compositors and to provide a generic X.Org input driver."
)
