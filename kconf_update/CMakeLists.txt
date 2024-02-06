# SPDX-FileCopyrightText: 2023 Niccolò Venerandi <niccolo.venerandi@kde.org>
# SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

add_executable(kwin-6.0-delete-desktop-switching-shortcuts)
target_sources(kwin-6.0-delete-desktop-switching-shortcuts PRIVATE kwin-6.0-delete-desktop-switching-shortcuts.cpp)
target_link_libraries(kwin-6.0-delete-desktop-switching-shortcuts PRIVATE KF6::GlobalAccel)
install(TARGETS kwin-6.0-delete-desktop-switching-shortcuts DESTINATION ${KDE_INSTALL_LIBDIR}/kconf_update_bin/)

add_executable(kwin-6.0-reset-active-mouse-screen)
target_sources(kwin-6.0-reset-active-mouse-screen PRIVATE kwin-6.0-reset-active-mouse-screen.cpp)
target_link_libraries(kwin-6.0-reset-active-mouse-screen PRIVATE KF6::ConfigCore)
install(TARGETS kwin-6.0-reset-active-mouse-screen DESTINATION ${KDE_INSTALL_LIBDIR}/kconf_update_bin/)

install(FILES kwin.upd
        DESTINATION ${KDE_INSTALL_KCONFUPDATEDIR})
