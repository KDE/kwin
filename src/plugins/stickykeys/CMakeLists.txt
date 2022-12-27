# SPDX-FileCopyrightText: 2022 Nicolas Fella <nicolas.fella@gmx.de>
# SPDX-License-Identifier: BSD-3-Clause

kcoreaddons_add_plugin(StickyKeysPlugin INSTALL_NAMESPACE "kwin/plugins")

ecm_qt_declare_logging_category(StickyKeysPlugin
    HEADER stickykeys_debug.h
    IDENTIFIER KWIN_STICKYKEYS
    CATEGORY_NAME kwin_stickykeys
    DEFAULT_SEVERITY Warning
)

target_sources(StickyKeysPlugin PRIVATE
    main.cpp
    stickykeys.cpp
)
target_link_libraries(StickyKeysPlugin PRIVATE kwin KF6::WindowSystem)

