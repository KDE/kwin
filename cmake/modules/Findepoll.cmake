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
# SPDX-FileCopyrightText: 2019 Tobias C. Berner <tcberner@FreeBSD.org>
#
# SPDX-License-Identifier: BSD-2-Clause
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
