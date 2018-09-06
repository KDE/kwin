#.rst:
# FindQt5FontDatabaseSupport
# -------
#
# Try to find Qt5FontDatabaseSupport on a Unix system.
#
# This will define the following variables:
#
# ``Qt5FontDatabaseSupport_FOUND``
#     True if (the requested version of) Qt5FontDatabaseSupport is available
# ``Qt5FontDatabaseSupport_VERSION``
#     The version of Qt5FontDatabaseSupport
# ``Qt5FontDatabaseSupport_LIBRARIES``
#     This can be passed to target_link_libraries() instead of the ``Qt5FontDatabaseSupport::Qt5FontDatabaseSupport``
#     target
# ``Qt5FontDatabaseSupport_INCLUDE_DIRS``
#     This should be passed to target_include_directories() if the target is not
#     used for linking
# ``Qt5FontDatabaseSupport_DEFINITIONS``
#     This should be passed to target_compile_options() if the target is not
#     used for linking
#
# If ``Qt5FontDatabaseSupport_FOUND`` is TRUE, it will also define the following imported target:
#
# ``Qt5FontDatabaseSupport::Qt5FontDatabaseSupport``
#     The Qt5FontDatabaseSupport library
#
# In general we recommend using the imported target, as it is easier to use.
# Bear in mind, however, that if the target is in the link interface of an
# exported library, it must be made available by the package config file.

#=============================================================================
# Copyright 2014 Alex Merry <alex.merry@kde.org>
# Copyright 2014 Martin Gräßlin <mgraesslin@kde.org>
# Copyright 2016 Takahiro Hashimoto <kenya888@gmail.com>
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
    message(FATAL_ERROR "CMake 2.8.12 is required by FindQt5FontDatabaseSupport.cmake")
endif()
if(CMAKE_MINIMUM_REQUIRED_VERSION VERSION_LESS 2.8.12)
    message(AUTHOR_WARNING "Your project should require at least CMake 2.8.12 to use FindQt5FontDatabaseSupport.cmake")
endif()

#Trying to find in the default paths
find_path(Qt5FontDatabaseSupport_INCLUDE_DIR
    NAMES
        QtFontDatabaseSupport/private/qfontconfigdatabase_p.h
    PATH_SUFFIXES
        QtFontDatabaseSupport/${Qt5Core_VERSION}/
)

if (Qt5FontDatabaseSupport_INCLUDE_DIR)
	set(Qt5FontDatabaseSupport_VERSION ${Qt5Core_VERSION})
else()
	# Use pkg-config to get the directories and then use these values
	# in the FIND_PATH() and FIND_LIBRARY() calls
	find_package(PkgConfig)
	pkg_check_modules(PKG_Qt5FontDatabaseSupport QUIET Qt5Gui)

	set(Qt5FontDatabaseSupport_DEFINITIONS ${PKG_Qt5FontDatabaseSupport_CFLAGS_OTHER})
	set(Qt5FontDatabaseSupport_VERSION ${PKG_Qt5FontDatabaseSupport_VERSION})
	find_path(Qt5FontDatabaseSupport_INCLUDE_DIR
	    NAMES
		QtFontDatabaseSupport/private/qfontconfigdatabase_p.h
	    HINTS
	        ${PKG_Qt5FontDatabaseSupport_INCLUDEDIR}/QtFontDatabaseSupport/${PKG_Qt5FontDatabaseSupport_VERSION}/
	)
endif()

find_library(Qt5FontDatabaseSupport_LIBRARY
    NAMES
        Qt5FontDatabaseSupport
    HINTS
        ${PKG_Qt5FontDatabaseSupport_LIBRARY_DIRS}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Qt5FontDatabaseSupport
    FOUND_VAR
        Qt5FontDatabaseSupport_FOUND
    REQUIRED_VARS
        Qt5FontDatabaseSupport_LIBRARY
        Qt5FontDatabaseSupport_INCLUDE_DIR
    VERSION_VAR
        Qt5FontDatabaseSupport_VERSION
)

if(Qt5FontDatabaseSupport_FOUND AND NOT TARGET Qt5FontDatabaseSupport::Qt5FontDatabaseSupport)
    add_library(Qt5FontDatabaseSupport::Qt5FontDatabaseSupport UNKNOWN IMPORTED)
    set_target_properties(Qt5FontDatabaseSupport::Qt5FontDatabaseSupport PROPERTIES
        IMPORTED_LOCATION "${Qt5FontDatabaseSupport_LIBRARY}"
        INTERFACE_COMPILE_OPTIONS "${Qt5FontDatabaseSupport_DEFINITIONS}"
        INTERFACE_INCLUDE_DIRECTORIES "${Qt5FontDatabaseSupport_INCLUDE_DIR}"
    )
endif()

mark_as_advanced(Qt5FontDatabaseSupport_LIBRARY Qt5FontDatabaseSupport_INCLUDE_DIR)

# compatibility variables
set(Qt5FontDatabaseSupport_LIBRARIES ${Qt5FontDatabaseSupport_LIBRARY})
set(Qt5FontDatabaseSupport_INCLUDE_DIRS ${Qt5FontDatabaseSupport_INCLUDE_DIR})
set(Qt5FontDatabaseSupport_VERSION_STRING ${Qt5FontDatabaseSupport_VERSION})


include(FeatureSummary)
set_package_properties(Qt5FontDatabaseSupport PROPERTIES
    URL "http://www.qt.io"
    DESCRIPTION "Qt FontDatabaseSupport module."
)

