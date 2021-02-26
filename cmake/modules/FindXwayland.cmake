#.rst:
# FindXwayland
# -------
#
# Try to find Xwayland on a Unix system.
#
# This will define the following variables:
#
# ``Xwayland_FOUND``
#     True if (the requested version of) Xwayland is available
# ``Xwayland_VERSION``
#     The version of Xwayland
# ``Xwayland_HAVE_LISTENFD``
#     True if (the requested version of) Xwayland has -listenfd option

#=============================================================================
# SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>
# SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
#
# SPDX-License-Identifier: BSD-3-Clause
#=============================================================================

find_package(PkgConfig)
pkg_check_modules(PKG_xwayland QUIET xwayland)

set(Xwayland_VERSION ${PKG_xwayland_VERSION})
pkg_get_variable(Xwayland_HAVE_LISTENFD xwayland have_listenfd)

find_program(Xwayland_EXECUTABLE NAMES Xwayland)
find_package_handle_standard_args(Xwayland
    FOUND_VAR Xwayland_FOUND
    REQUIRED_VARS Xwayland_EXECUTABLE
    VERSION_VAR Xwayland_VERSION
)
mark_as_advanced(
    Xwayland_EXECUTABLE
    Xwayland_HAVE_LISTENFD
    Xwayland_VERSION
)
