#.rest:
# FindEpoll
# --------------
#
# Try to find epoll or epoll-shim on this system. This finds:
#  - some shim on Unix like systems (FreeBSD), or
#  - the kernel's epoll on Linux systems.
#
# This will define the following variables:
#
# ``epoll_FOUND``
#    True if epoll is available
# ``epoll_LIBRARIES``
#    This has to be passed to target_link_libraries()
# ``epoll_INCLUDE_DIRS``
#    This has to be passed to target_include_directories()
#
# On Linux, the libraries and include directories are empty,
# even though epoll_FOUND may be set to TRUE. This is because
# no special includes or libraries are needed. On other systems
# these may be needed to use epoll.

#=============================================================================
# Copyright 2019 Tobias C. Berner <tcberner@FreeBSD.org>
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

find_path(epoll_INCLUDE_DIRS sys/epoll.h PATH_SUFFIXES libepoll-shim)

if(epoll_INCLUDE_DIRS)
    # On Linux there is no library to link against, on the BSDs there is.
    # On the BSD's, epoll is implemented through a library, libepoll-shim.
    if( CMAKE_SYSTEM_NAME MATCHES "Linux")
        set(epoll_FOUND TRUE)
        set(epoll_LIBRARIES "")
        set(epoll_INCLUDE_DIRS "")
    else()
        find_library(epoll_LIBRARIES NAMES epoll-shim)
        include(FindPackageHandleStandardArgs)
        find_package_handle_standard_args(epoll
            FOUND_VAR
                epoll_FOUND
            REQUIRED_VARS
                epoll_LIBRARIES
                epoll_INCLUDE_DIRS
        )
        mark_as_advanced(epoll_LIBRARIES epoll_INCLUDE_DIRS)
        include(FeatureSummary)
        set_package_properties(epoll PROPERTIES
            URL "https://github.com/FreeBSDDesktop/epoll-shim"
            DESCRIPTION "small epoll implementation using kqueue"
        )
    endif()
else()
   set(epoll_FOUND FALSE)
endif()

mark_as_advanced(epoll_LIBRARIES epoll_INCLUDE_DIRS)
