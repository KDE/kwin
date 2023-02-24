# SPDX-FileCopyrightText: 2022 Marco Martin <mart@kde.org>
#
# SPDX-License-Identifier: BSD-3-Clause

set(kwin_tileseditor_config_SOURCES tileseditoreffectkcm.cpp)
ki18n_wrap_ui(kwin_tileseditor_config_SOURCES tileseditoreffectkcm.ui)

kwin_add_effect_config(kwin_tileseditor_config ${kwin_tileseditor_config_SOURCES})
target_link_libraries(kwin_tileseditor_config
    KF6::ConfigWidgets
    KF6::CoreAddons
    KF6::GlobalAccel
    KF6::I18n
    KF6::XmlGui
    KWinEffectsInterface
)
