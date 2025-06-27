# SPDX-FileCopyrightText: 2025 David Redondo <kde@david-redondo.de>
#
# SPDX-License-Identifier: BSD-3-Clause

kwin_add_builtin_effect(windowpicker windowpicker.cpp main.cpp)

qt_add_resources(windowpicker "windowpicker_qml" PREFIX "org.kde.kwin/windowpicker" FILES qml/main.qml)

target_link_libraries(windowpicker PRIVATE
    kwin
    Qt::Core
    Qt::Quick
    )
