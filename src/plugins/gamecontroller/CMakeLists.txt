# SPDX-FileCopyrightText: 2025 Yelsin Sepulveda <yelsin.sepulveda@kdemail.net>
# SPDX-License-Identifier: BSD-3-Clause

kcoreaddons_add_plugin(gamecontroller INSTALL_NAMESPACE "kwin/plugins")

ecm_qt_declare_logging_category(gamecontroller
    HEADER gamecontroller_logging.h
    IDENTIFIER KWIN_GAMECONTROLLER
    CATEGORY_NAME kwin_gamecontroller
    DEFAULT_SEVERITY Debug
)

target_sources(gamecontroller PRIVATE
    main.cpp
    gamecontrollermanager.cpp
    gamecontroller.cpp
    emulatedinputdevice.cpp
)

target_link_libraries(gamecontroller PRIVATE
    kwin

    KF6::I18n

    PkgConfig::libevdev
    XKB::XKB
)

