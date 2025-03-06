# SPDX-FileCopyrightText: 2025 Nicolas Fella <nicolas.fella@gmx.de>
# SPDX-License-Identifier: BSD-3-Clause

kcoreaddons_add_plugin(A11yKeyboardMonitorPlugin INSTALL_NAMESPACE "${KWIN_PLUGINDIR}/plugins")

ecm_qt_declare_logging_category(A11yKeyboardMonitorPlugin
    HEADER stickykeys_debug.h
    IDENTIFIER KWIN_A11YKEYS
    CATEGORY_NAME kwin_a11ykeys
    DEFAULT_SEVERITY Warning
)

target_sources(A11yKeyboardMonitorPlugin PRIVATE
    main.cpp
    a11ykeyboardmonitor.cpp
)
target_link_libraries(A11yKeyboardMonitorPlugin PRIVATE kwin XKB::XKB)

