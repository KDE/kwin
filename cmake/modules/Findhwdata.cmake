# - Try to find hwdata
# Once done this will define
#
#  hwdata_DIR         - The hwdata directory
#  hwdata_PNPIDS_FILE - File with mapping of hw vendor IDs to names
#  hwdata_FOUND       - The hwdata directory exists and contains pnp.ids file

# SPDX-FileCopyrightText: 2020 Daniel Vrátil <dvratil@kde.org>
#
# SPDX-License-Identifier: BSD-3-Clause

if (UNIX AND NOT APPLE)
    find_path(hwdata_DIR NAMES hwdata/pnp.ids HINTS /usr/share ENV XDG_DATA_DIRS)
    find_file(hwdata_PNPIDS_FILE NAMES hwdata/pnp.ids HINTS /usr/share)
    if (NOT hwdata_DIR OR NOT hwdata_PNPIDS_FILE)
        set(hwdata_FOUND FALSE)
    else()
        set(hwdata_FOUND TRUE)
    endif()

    include(FindPackageHandleStandardArgs)
    find_package_handle_standard_args(hwdata DEFAULT_MSG hwdata_FOUND hwdata_DIR hwdata_PNPIDS_FILE)

    mark_as_advanced(hwdata_FOUND hwdata_DIR hwdata_PNPIDS_FILE)
endif()
