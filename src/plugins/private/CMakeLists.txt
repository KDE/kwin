# SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
#
# SPDX-License-Identifier: BSD-3-Clause

ecm_add_qml_module(effectsplugin URI "org.kde.kwin.private.effects")

ecm_target_qml_sources(effectsplugin
    SOURCES
        qml/WindowHeap.qml
        qml/WindowHeapDelegate.qml
)

target_sources(effectsplugin PRIVATE
    expoarea.cpp
    expolayout.cpp
    plugin.cpp
)

target_link_libraries(effectsplugin PRIVATE
    kwin
    Qt6::Quick
    Qt6::Qml
    KF6::I18n
)

ecm_finalize_qml_module(effectsplugin DESTINATION ${KDE_INSTALL_QMLDIR})

