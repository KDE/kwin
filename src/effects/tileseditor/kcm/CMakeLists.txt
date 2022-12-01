# SPDX-FileCopyrightText: 2022 Marco Martin <mart@kde.org>
#
# SPDX-License-Identifier: BSD-3-Clause

set(kwin_tileseditor_config_SOURCES tileseditoreffectkcm.cpp)
ki18n_wrap_ui(kwin_tileseditor_config_SOURCES tileseditoreffectkcm.ui)

kwin_add_effect_config(kwin_tileseditor_config ${kwin_tileseditor_config_SOURCES})
target_link_libraries(kwin_tileseditor_config
    KF5::ConfigWidgets
    KF5::CoreAddons
    KF5::GlobalAccel
    KF5::I18n
    KF5::XmlGui
    KWinEffectsInterface
)
