#=============================================================================
# SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>
#
# SPDX-License-Identifier: BSD-3-Clause
#=============================================================================
find_program(Xwayland_EXECUTABLE NAMES Xwayland)
find_package_handle_standard_args(Xwayland
    FOUND_VAR
        Xwayland_FOUND
    REQUIRED_VARS
        Xwayland_EXECUTABLE
)
mark_as_advanced(Xwayland_EXECUTABLE)
