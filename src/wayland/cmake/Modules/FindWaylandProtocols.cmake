#.rst:
# FindWaylandProtocols
# -------
#
# Try to find wayland-protocols on a Unix system.
#
# This will define the following variables:
#
# ``WaylandProtocols_FOUND``
#     True if (the requested version of) wayland-protocols is available
# ``WaylandProtocols_VERSION``
#     The version of wayland-protocols
# ``WaylandProtocols_DATADIR``
#     The wayland protocols data directory

#=============================================================================
# Copyright 2019 Vlad Zagorodniy <vlad.zahorodnii@kde.org>
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. The name of the author may not be used to endorse or promote products
#    derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
# NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#=============================================================================

find_package(PkgConfig)
pkg_check_modules(PKG_wayland_protocols QUIET wayland-protocols)

set(WaylandProtocols_VERSION ${PKG_wayland_protocols_VERSION})
pkg_get_variable(WaylandProtocols_DATADIR wayland-protocols pkgdatadir)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(WaylandProtocols
    FOUND_VAR WaylandProtocols_FOUND
    REQUIRED_VARS WaylandProtocols_DATADIR
    VERSION_VAR WaylandProtocols_VERSION
)

include(FeatureSummary)
set_package_properties(WaylandProtocols PROPERTIES
    DESCRIPTION "Specifications of extended Wayland protocols"
    URL "https://wayland.freedesktop.org/"
)
