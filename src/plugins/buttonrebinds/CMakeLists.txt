# SPDX-FileCopyrightText: 2022 David Redondo <kde@david-redono.de>
# SPDX-License-Identifier: BSD-3-Clause

kcoreaddons_add_plugin(buttonsrebind INSTALL_NAMESPACE "kwin/plugins")

ecm_qt_declare_logging_category(buttonsrebind
    HEADER buttonrebinds_debug.h
    IDENTIFIER KWIN_BUTTONREBINDS
    CATEGORY_NAME kwin_buttonrebinds
    DEFAULT_SEVERITY Warning
)

target_sources(buttonsrebind PRIVATE
    main.cpp
    buttonrebindsfilter.cpp
)
target_link_libraries(buttonsrebind PRIVATE kwin Qt::GuiPrivate XKB::XKB)
