# SPDX-FileCopyrightText: 2023 Nicolas Fella <nicolas.fella@gmx.de>
# SPDX-License-Identifier: BSD-3-Clause

kcoreaddons_add_plugin(BounceKeysPlugin INSTALL_NAMESPACE "kwin/plugins")

ecm_qt_declare_logging_category(BounceKeysPlugin
    HEADER bouncekeys_debug.h
    IDENTIFIER KWIN_BOUNCEKEYS
    CATEGORY_NAME kwin_bouncekeys
    DEFAULT_SEVERITY Warning
)

target_sources(BounceKeysPlugin PRIVATE
    main.cpp
    bouncekeys.cpp
)
target_link_libraries(BounceKeysPlugin PRIVATE kwin KF6::WindowSystem)

