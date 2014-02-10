# - Try to find libepoxy
# Once done this will define
#
#  epoxy_FOUND        - System has libepoxy
#  epoxy_LIBRARY      - The libepoxy library
#  epoxy_INCLUDE_DIR  - The libepoxy include dir
#  epoxy_DEFINITIONS  - Compiler switches required for using libepoxy

# Copyright (c) 2014 Fredrik Höglund <fredrik@kde.org>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

if (NOT WIN32)
  find_package(PkgConfig)
  pkg_check_modules(PKG_epoxy QUIET epoxy)

  set(epoxy_DEFINITIONS ${PKG_epoxy_CFLAGS})

  find_path(epoxy_INCLUDE_DIR NAMES epoxy/gl.h HINTS ${PKG_epoxy_INCLUDEDIR} ${PKG_epoxy_INCLUDE_DIRS})
  find_library(epoxy_LIBRARY  NAMES epoxy      HINTS ${PKG_epoxy_LIBDIR} ${PKG_epoxy_LIBRARY_DIRS})

  include(FindPackageHandleStandardArgs)
  find_package_handle_standard_args(epoxy DEFAULT_MSG epoxy_LIBRARY epoxy_INCLUDE_DIR)

  mark_as_advanced(epoxy_INCLUDE_DIR epoxy_LIBRARY)
endif()
