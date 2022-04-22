# SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
#
# SPDX-License-Identifier: BSD-3-Clause

add_library(effectsplugin
    expoarea.cpp
    expolayout.cpp
    plugin.cpp
)

target_link_libraries(effectsplugin
    kwineffects

    Qt::Core
    Qt::Gui
    Qt::Qml
    Qt::Quick
)

install(DIRECTORY qml/ DESTINATION ${KDE_INSTALL_QMLDIR}/org/kde/kwin/private/effects)
install(TARGETS effectsplugin DESTINATION ${KDE_INSTALL_QMLDIR}/org/kde/kwin/private/effects)
install(FILES qmldir DESTINATION ${KDE_INSTALL_QMLDIR}/org/kde/kwin/private/effects)
