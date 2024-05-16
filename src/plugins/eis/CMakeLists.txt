# SPDX-FileCopyrightText: 2024 David Redondo <kde@david-redono.de>
# SPDX-License-Identifier: BSD-3-Clause

kcoreaddons_add_plugin(eis INSTALL_NAMESPACE "kwin/plugins")

ecm_qt_declare_logging_category(eis
    HEADER libeis_logging.h
    IDENTIFIER KWIN_EIS
    CATEGORY_NAME kwin_libeis
    DEFAULT_SEVERITY Debug
)

ecm_qt_declare_logging_category(eis
    HEADER inputcapture_logging.h
    IDENTIFIER KWIN_INPUTCAPTURE
    CATEGORY_NAME kwin_inputcapture
    DEFAULT_SEVERITY Debug
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

target_link_libraries(eis PRIVATE kwin Libeis::Libeis XKB::XKB)
