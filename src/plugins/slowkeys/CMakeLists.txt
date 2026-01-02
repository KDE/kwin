# SPDX-FileCopyrightText: 2025 Martin Riethmayer <ripper@freakmail.de>
# SPDX-License-Identifier: BSD-3-Clause

kcoreaddons_add_plugin(SlowKeysPlugin INSTALL_NAMESPACE "kwin/plugins")

ecm_qt_declare_logging_category(SlowKeysPlugin
    HEADER slowkeys_debug.h
    IDENTIFIER KWIN_SLOWKEYS
    CATEGORY_NAME kwin_slowkeys
    DEFAULT_SEVERITY Warning
)

target_sources(SlowKeysPlugin PRIVATE
    main.cpp
    slowkeys.cpp
)
target_link_libraries(SlowKeysPlugin PRIVATE kwin KF6::WindowSystem)

