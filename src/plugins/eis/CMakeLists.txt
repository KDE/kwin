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
)

target_link_libraries(eis PRIVATE kwin KF6::I18n KF6::GlobalAccel KF6::ConfigGui Libeis::Libeis XKB::XKB)

if (Libeis_VERSION VERSION_GREATER_EQUAL "1.4")
    set(EIS_HAVE_TOUCH_CANCEL 1)
    set(EIS_HAVE_PONG_AND_SYNC 1)
endif()
configure_file(config-eis.h.in config-eis.h)
