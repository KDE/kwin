#.rst:
# Findseatd
# -------
#
# Try to find seatd on a Unix system.
#
# This will define the following variables:
#
# ``seatd_FOUND``
#     True if (the requested version of) libseat is available
# ``seatd_VERSION``
#     The version of libseat
# ``seatd_LIBRARIES``
#     This should be passed to target_link_libraries() if the target is not
#     used for linking
# ``seatd_INCLUDE_DIRS``
#     This should be passed to target_include_directories() if the target is not
#     used for linking
# ``seatd_DEFINITIONS``
#     This should be passed to target_compile_options() if the target is not
#     used for linking
#
# If ``seatd_FOUND`` is TRUE, it will also define the following imported target:
#
# ``seatd::seatd``
#     The libseat library
#
# In general we recommend using the imported target, as it is easier to use.
# Bear in mind, however, that if the target is in the link interface of an
# exported library, it must be made available by the package config file.

# SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
# SPDX-License-Identifier: BSD-3-Clause

find_package(PkgConfig)
pkg_check_modules(PKG_libseat QUIET libseat)

set(seatd_VERSION ${PKG_libseat_VERSION})
set(seatd_DEFINITIONS ${PKG_libseat_CFLAGS_OTHER})

find_path(seatd_INCLUDE_DIR
    NAMES libseat.h
    HINTS ${PKG_libseat_INCLUDE_DIRS}
)

find_library(seatd_LIBRARY
    NAMES seat
    HINTS ${PKG_libseat_LIBRARY_DIRS}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(seatd
    FOUND_VAR seatd_FOUND
    REQUIRED_VARS seatd_LIBRARY
                  seatd_INCLUDE_DIR
    VERSION_VAR seatd_VERSION
)

if (seatd_FOUND AND NOT TARGET seatd::seatd)
    add_library(seatd::seatd UNKNOWN IMPORTED)
    set_target_properties(seatd::seatd PROPERTIES
        IMPORTED_LOCATION "${seatd_LIBRARY}"
        INTERFACE_COMPILE_OPTIONS "${seatd_DEFINITIONS}"
        INTERFACE_INCLUDE_DIRECTORIES "${seatd_INCLUDE_DIR}"
    )
endif()

set(seatd_INCLUDE_DIRS ${seatd_INCLUDE_DIR})
set(seatd_LIBRARIES ${seatd_LIBRARY})

mark_as_advanced(seatd_INCLUDE_DIR)
mark_as_advanced(seatd_LIBRARY)
