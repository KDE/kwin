# SPDX-FileCopyrightText: 2022 David Redondo <kde@david-redondo.de>
#
# SPDX-License-Identifier: BSD-3-Clause


kwin_add_builtin_effect(outputlocator main.cpp outputlocator.cpp)

ecm_add_qml_module(outputlocator
    URI org.kde.kwin.outputlocator
    QML_FILES
        qml/OutputLabel.qml
    QT_NO_PLUGIN
)

target_link_libraries(outputlocator PRIVATE
    kwin
    Qt::DBus
    Qt::Quick
    KF6::I18n
)
