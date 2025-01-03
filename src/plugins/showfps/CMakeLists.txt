# SPDX-FileCopyrightText: 2022 Arjen Hiemstra <ahiemstra@heimr.nl>
#
# SPDX-License-Identifier: BSD-3-Clause

set(showfps_SOURCES
    main.cpp
    showfpseffect.cpp
)

kwin_add_builtin_effect(showfps ${showfps_SOURCES})

target_link_libraries(showfps PRIVATE
    kwin

    KF6::I18n

    Qt::Quick
    )

install(DIRECTORY qml DESTINATION ${KDE_INSTALL_DATADIR}/${KWIN_DATADIR}/effects/showfps)
