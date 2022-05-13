#.rst:
# Findlcms2
# -------
#
# Try to find lcms2 on a Unix system.
#
# This will define the following variables:
#
# ``lcms2_FOUND``
#     True if (the requested version of) lcms2 is available
# ``lcms2_VERSION``
#     The version of lcms2
# ``lcms2_LIBRARIES``
#     This should be passed to target_link_libraries() if the target is not
#     used for linking
# ``lcms2_INCLUDE_DIRS``
#     This should be passed to target_include_directories() if the target is not
#     used for linking
# ``lcms2_DEFINITIONS``
#     This should be passed to target_compile_options() if the target is not
#     used for linking
#
# If ``lcms2_FOUND`` is TRUE, it will also define the following imported target:
#
# ``lcms2::lcms2``
#     The lcms2 library
#
# In general we recommend using the imported target, as it is easier to use.
# Bear in mind, however, that if the target is in the link interface of an
# exported library, it must be made available by the package config file.

# SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
# SPDX-License-Identifier: BSD-3-Clause

find_package(PkgConfig)
pkg_check_modules(PKG_lcms2 QUIET lcms2)

set(lcms2_VERSION ${PKG_lcms2_VERSION})
set(lcms2_DEFINITIONS ${PKG_lcms2_CFLAGS_OTHER})

find_path(lcms2_INCLUDE_DIR
    NAMES lcms2.h
    HINTS ${PKG_lcms2_INCLUDE_DIRS}
)

find_library(lcms2_LIBRARY
    NAMES lcms2
    HINTS ${PKG_lcms2_LIBRARY_DIRS}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(lcms2
    FOUND_VAR lcms2_FOUND
    REQUIRED_VARS lcms2_LIBRARY
                  lcms2_INCLUDE_DIR
    VERSION_VAR lcms2_VERSION
)

if (lcms2_FOUND AND NOT TARGET lcms2::lcms2)
    add_library(lcms2::lcms2 UNKNOWN IMPORTED)
    set_target_properties(lcms2::lcms2 PROPERTIES
        IMPORTED_LOCATION "${lcms2_LIBRARY}"
        INTERFACE_COMPILE_OPTIONS "${lcms2_DEFINITIONS}"
        # Don't use the register keyword to allow compiling in C++17 mode.
        # See https://github.com/mm2/Little-CMS/issues/243
        INTERFACE_COMPILE_DEFINITIONS "CMS_NO_REGISTER_KEYWORD=1"
        INTERFACE_INCLUDE_DIRECTORIES "${lcms2_INCLUDE_DIR}"
    )
endif()

set(lcms2_INCLUDE_DIRS ${lcms2_INCLUDE_DIR})
set(lcms2_LIBRARIES ${lcms2_LIBRARY})

mark_as_advanced(lcms2_INCLUDE_DIR)
mark_as_advanced(lcms2_LIBRARY)
