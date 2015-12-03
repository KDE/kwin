#.rst:
# FindQt5PlatformSupport
# -------
#
# Try to find Qt5PlatformSupport on a Unix system.
#
# This will define the following variables:
#
# ``Qt5PlatformSupport_FOUND``
#     True if (the requested version of) Qt5PlatformSupport is available
# ``Qt5PlatformSupport_VERSION``
#     The version of Qt5PlatformSupport
# ``Qt5PlatformSupport_LIBRARIES``
#     This can be passed to target_link_libraries() instead of the ``Qt5PlatformSupport::Qt5PlatformSupport``
#     target
# ``Qt5PlatformSupport_INCLUDE_DIRS``
#     This should be passed to target_include_directories() if the target is not
#     used for linking
# ``Qt5PlatformSupport_DEFINITIONS``
#     This should be passed to target_compile_options() if the target is not
#     used for linking
#
# If ``Qt5PlatformSupport_FOUND`` is TRUE, it will also define the following imported target:
#
# ``Qt5PlatformSupport::Qt5PlatformSupport``
#     The Qt5PlatformSupport library
#
# In general we recommend using the imported target, as it is easier to use.
# Bear in mind, however, that if the target is in the link interface of an
# exported library, it must be made available by the package config file.

#=============================================================================
# Copyright 2014 Alex Merry <alex.merry@kde.org>
# Copyright 2014 Martin Gräßlin <mgraesslin@kde.org>
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

if(CMAKE_VERSION VERSION_LESS 2.8.12)
    message(FATAL_ERROR "CMake 2.8.12 is required by FindQt5PlatformSupport.cmake")
endif()
if(CMAKE_MINIMUM_REQUIRED_VERSION VERSION_LESS 2.8.12)
    message(AUTHOR_WARNING "Your project should require at least CMake 2.8.12 to use FindQt5PlatformSupport.cmake")
endif()

# Use pkg-config to get the directories and then use these values
# in the FIND_PATH() and FIND_LIBRARY() calls
find_package(PkgConfig)
pkg_check_modules(PKG_Qt5PlatformSupport QUIET Qt5Gui)

set(Qt5PlatformSupport_DEFINITIONS ${PKG_Qt5PlatformSupport_CFLAGS_OTHER})
set(Qt5PlatformSupport_VERSION ${PKG_Qt5PlatformSupport_VERSION})

find_path(Qt5PlatformSupport_INCLUDE_DIR
    NAMES
        QtPlatformSupport/private/qfontconfigdatabase_p.h
    HINTS
        ${PKG_Qt5PlatformSupport_INCLUDEDIR}/QtPlatformSupport/${PKG_Qt5PlatformSupport_VERSION}/
)
find_library(Qt5PlatformSupport_LIBRARY
    NAMES
        Qt5PlatformSupport
    HINTS
        ${PKG_Qt5PlatformSupport_LIBRARY_DIRS}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Qt5PlatformSupport
    FOUND_VAR
        Qt5PlatformSupport_FOUND
    REQUIRED_VARS
        Qt5PlatformSupport_LIBRARY
        Qt5PlatformSupport_INCLUDE_DIR
    VERSION_VAR
        Qt5PlatformSupport_VERSION
)

if(Qt5PlatformSupport_FOUND AND NOT TARGET Qt5PlatformSupport::Qt5PlatformSupport)
    add_library(Qt5PlatformSupport::Qt5PlatformSupport UNKNOWN IMPORTED)
    set_target_properties(Qt5PlatformSupport::Qt5PlatformSupport PROPERTIES
        IMPORTED_LOCATION "${Qt5PlatformSupport_LIBRARY}"
        INTERFACE_COMPILE_OPTIONS "${Qt5PlatformSupport_DEFINITIONS}"
        INTERFACE_INCLUDE_DIRECTORIES "${Qt5PlatformSupport_INCLUDE_DIR}"
    )
endif()

mark_as_advanced(Qt5PlatformSupport_LIBRARY Qt5PlatformSupport_INCLUDE_DIR)

# compatibility variables
set(Qt5PlatformSupport_LIBRARIES ${Qt5PlatformSupport_LIBRARY})
set(Qt5PlatformSupport_INCLUDE_DIRS ${Qt5PlatformSupport_INCLUDE_DIR})
set(Qt5PlatformSupport_VERSION_STRING ${Qt5PlatformSupport_VERSION})


include(FeatureSummary)
set_package_properties(Qt5PlatformSupport PROPERTIES
    URL "http://www.qt.io"
    DESCRIPTION "Qt PlatformSupport module."
)

