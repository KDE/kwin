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
#     This should be passed to target_compile_options() if the target is not
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

# Copyright (C) 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. Neither the name of the University nor the names of its contributors
#    may be used to endorse or promote products derived from this software
#    without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.

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
        INTERFACE_INCLUDE_DIRECTORIES "${lcms2_INCLUDE_DIR}"
    )
endif()

set(lcms2_INCLUDE_DIRS ${lcms2_INCLUDE_DIR})
set(lcms2_LIBRARIES ${lcms2_LIBRARY})

mark_as_advanced(lcms2_INCLUDE_DIR)
mark_as_advanced(lcms2_LIBRARY)
