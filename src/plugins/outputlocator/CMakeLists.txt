# SPDX-FileCopyrightText: 2022 David Redondo <kde@david-redondo.de>
#
# SPDX-License-Identifier: BSD-3-Clause


kwin_add_builtin_effect(outputlocator main.cpp outputlocator.cpp)

qt_add_resources(outputlocator outputlocator_qml PREFIX "org.kde.kwin/outputlocator" FILES qml/OutputLabel.qml)

target_link_libraries(outputlocator PRIVATE
    kwin
    Qt::DBus
    Qt::Quick
    KF6::I18n
)
