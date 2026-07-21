# SPDX-FileCopyrightText: 2026 Nicolas Fella <nicolas.fella@gmx.de>
# SPDX-License-Identifier: BSD-3-Clause

kcoreaddons_add_plugin(DwellClickerPlugin INSTALL_NAMESPACE "kwin/plugins")

ecm_qt_declare_logging_category(DwellClickerPlugin
    HEADER dwellclicker_debug.h
    IDENTIFIER KWIN_DWELLCLICKER
    CATEGORY_NAME kwin_dwellclicker
)

target_sources(DwellClickerPlugin PRIVATE
    main.cpp
    dwellclicker.cpp
)
target_link_libraries(DwellClickerPlugin PRIVATE kwin)

