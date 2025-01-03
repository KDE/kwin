# SPDX-FileCopyrightText: 2022 Arjen Hiemstra <ahiemstra@heimr.nl>
# SPDX-FileCopyrightText: 2024 Xaver Hugl <xaver.hugl@gmail.com>
#
# SPDX-License-Identifier: BSD-3-Clause

set(showcompositing_SOURCES
    main.cpp
    showcompositing.cpp
)

kwin_add_builtin_effect(showcompositing ${showcompositing_SOURCES})

target_link_libraries(showcompositing PRIVATE
    kwin
    KF6::I18n
    Qt::Quick
)

install(DIRECTORY qml DESTINATION ${KDE_INSTALL_DATADIR}/${KWIN_DATADIR}/effects/showcompositing)
