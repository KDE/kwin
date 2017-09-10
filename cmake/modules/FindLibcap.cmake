# Try to find the setcap binary and cap libraries
#
# This will define:
#
#   Libcap_FOUND           - system has the cap library and setcap binary
#   Libcap_LIBRARIES       - cap libraries to link against
#   SETCAP_EXECUTABLE      - path of the setcap binary
# In addition, the following targets are defined:
#
#   Libcap::SetCapabilities
#


# Copyright (c) 2014, Hrvoje Senjan, <hrvoje.senjan@gmail.com>
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

find_program(SETCAP_EXECUTABLE NAMES setcap DOC "The setcap executable")

find_library(Libcap_LIBRARIES NAMES cap DOC "The cap (capabilities) library")

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Libcap FOUND_VAR Libcap_FOUND
                                      REQUIRED_VARS SETCAP_EXECUTABLE Libcap_LIBRARIES)

if(Libcap_FOUND AND NOT TARGET Libcap::SetCapabilities)
    add_executable(Libcap::SetCapabilities IMPORTED)
    set_target_properties(Libcap::SetCapabilities PROPERTIES
        IMPORTED_LOCATION "${SETCAP_EXECUTABLE}"
    )
endif()

mark_as_advanced(SETCAP_EXECUTABLE Libcap_LIBRARIES)

include(FeatureSummary)
set_package_properties(Libcap PROPERTIES
    URL https://sites.google.com/site/fullycapable/
    DESCRIPTION "Capabilities are a measure to limit the omnipotence of the superuser.")
