#.rst:
# FindFontconfig
# --------------
#
# Try to find Fontconfig.
# Once done this will define the following variables:
#
# ``Fontconfig_FOUND``
#     True if Fontconfig is available
# ``Fontconfig_INCLUDE_DIRS``
#     The include directory to use for the Fontconfig headers
# ``Fontconfig_LIBRARIES``
#     The Fontconfig libraries for linking
# ``Fontconfig_DEFINITIONS``
#     Compiler switches required for using Fontconfig
# ``Fontconfig_VERSION``
#     The version of Fontconfig that has been found
#
# If ``Fontconfig_FOUND`` is True, it will also define the following
# imported target:
#
# ``Fontconfig::Fontconfig``

#=============================================================================
# Copyright (c) 2006,2007 Laurent Montel, <montel@kde.org>
# Copyright (c) 2018 Volker Krause <vkrause@kde.org>
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

# use pkg-config to get the directories and then use these values
# in the FIND_PATH() and FIND_LIBRARY() calls
find_package(PkgConfig)
pkg_check_modules(PC_FONTCONFIG QUIET fontconfig)

set(Fontconfig_DEFINITIONS ${PC_FONTCONFIG_CFLAGS_OTHER})

find_path(Fontconfig_INCLUDE_DIRS fontconfig/fontconfig.h
  PATHS
  ${PC_FONTCONFIG_INCLUDE_DIRS}
  /usr/X11/include
)

find_library(Fontconfig_LIBRARIES NAMES fontconfig
  PATHS
  ${PC_FONTCONFIG_LIBRARY_DIRS}
)

set(Fontconfig_VERSION ${PC_FONTCONFIG_VERSION})
if (NOT Fontconfig_VERSION)
  find_file(Fontconfig_VERSION_HEADER
    NAMES "fontconfig/fontconfig.h"
    HINTS ${Fontconfig_INCLUDE_DIRS}
  )
  mark_as_advanced(Fontconfig_VERSION_HEADER)
  if (Fontconfig_VERSION_HEADER)
    file(READ ${Fontconfig_VERSION_HEADER} _fontconfig_version_header_content)
    string(REGEX MATCH "#define FC_MAJOR[ \t]+[0-9]+" Fontconfig_MAJOR_VERSION_MATCH ${_fontconfig_version_header_content})
    string(REGEX MATCH "#define FC_MINOR[ \t]+[0-9]+" Fontconfig_MINOR_VERSION_MATCH ${_fontconfig_version_header_content})
    string(REGEX MATCH "#define FC_REVISION[ \t]+[0-9]+" Fontconfig_PATCH_VERSION_MATCH ${_fontconfig_version_header_content})
    string(REGEX REPLACE ".*FC_MAJOR[ \t]+(.*)" "\\1" Fontconfig_MAJOR_VERSION ${Fontconfig_MAJOR_VERSION_MATCH})
    string(REGEX REPLACE ".*FC_MINOR[ \t]+(.*)" "\\1" Fontconfig_MINOR_VERSION ${Fontconfig_MINOR_VERSION_MATCH})
    string(REGEX REPLACE ".*FC_REVISION[ \t]+(.*)" "\\1" Fontconfig_PATCH_VERSION ${Fontconfig_PATCH_VERSION_MATCH})
    set(Fontconfig_VERSION "${Fontconfig_MAJOR_VERSION}.${Fontconfig_MINOR_VERSION}.${Fontconfig_PATCH_VERSION}")
  endif()
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Fontconfig
  FOUND_VAR Fontconfig_FOUND
  REQUIRED_VARS Fontconfig_LIBRARIES Fontconfig_INCLUDE_DIRS
  VERSION_VAR Fontconfig_VERSION
)
mark_as_advanced(Fontconfig_LIBRARIES Fontconfig_INCLUDE_DIRS)

if(Fontconfig_FOUND AND NOT TARGET Fontconfig::Fontconfig)
  add_library(Fontconfig::Fontconfig UNKNOWN IMPORTED)
  set_target_properties(Fontconfig::Fontconfig PROPERTIES
    IMPORTED_LOCATION "${Fontconfig_LIBRARIES}"
    INTERFACE_INCLUDE_DIRECTORIES "${Fontconfig_INCLUDE_DIRS}"
    INTERFACE_COMPILER_DEFINITIONS "${Fontconfig_DEFINITIONS}"
  )
endif()

include(FeatureSummary)
set_package_properties(Fontconfig PROPERTIES
  URL "https://www.fontconfig.org/"
  DESCRIPTION "Fontconfig is a library for configuring and customizing font access"
)
