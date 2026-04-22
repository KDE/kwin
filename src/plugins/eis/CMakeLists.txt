# SPDX-FileCopyrightText: 2024 David Redondo <kde@david-redono.de>
# SPDX-License-Identifier: BSD-3-Clause

kcoreaddons_add_plugin(eis INSTALL_NAMESPACE "kwin/plugins")

ecm_qt_declare_logging_category(eis
    HEADER libeis_logging.h
    IDENTIFIER KWIN_EIS
    CATEGORY_NAME kwin_libeis
    DEFAULT_SEVERITY Warning
)

ecm_qt_declare_logging_category(eis
    HEADER inputcapture_logging.h
    IDENTIFIER KWIN_INPUTCAPTURE
    CATEGORY_NAME kwin_inputcapture
    DEFAULT_SEVERITY Warning
)

target_sources(eis PRIVATE
    main.cpp
    eisdevice.cpp
    eisbackend.cpp
    eiscontext.cpp
    eisplugin.cpp
    eisinputcapture.cpp
    eisinputcapturemanager.cpp
    eisinputcapturefilter.cpp
    xwaylandeiscontext.cpp
)

target_link_libraries(eis PRIVATE kwin KF6::I18n KF6::GlobalAccel KF6::ConfigGui Libeis::Libeis XKB::XKB)

add_executable(kwin_eis_prompter eisprompter.cpp)
kconfig_target_kcfg_file(kwin_eis_prompter
    FILE ${PROJECT_SOURCE_DIR}/src/kcms/xwayland/kwinxwaylandsettings.kcfg
    CLASS_NAME XwaylandSettings
    MUTATORS
    NOTIFIERS
)
target_link_libraries(kwin_eis_prompter PRIVATE  KF6::I18n  KF6::ConfigGui KF6::WidgetsAddons KF6::Service)
install(TARGETS kwin_eis_prompter DESTINATION ${KDE_INSTALL_LIBEXECDIR})

set(PROMPTER_BIN ${KDE_INSTALL_FULL_LIBEXECDIR}/kwin_eis_prompter)

if (Libeis_VERSION VERSION_GREATER_EQUAL "1.5")
    set(EIS_HAVE_GET_CLIENT_PID 1)
endif()
configure_file(config-eis.h.in config-eis.h)
