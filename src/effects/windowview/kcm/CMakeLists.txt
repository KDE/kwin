# SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
#
# SPDX-License-Identifier: BSD-3-Clause

set(kwin_windowview_config_SOURCES windowvieweffectkcm.cpp)
ki18n_wrap_ui(kwin_windowview_config_SOURCES windowvieweffectkcm.ui)
kconfig_add_kcfg_files(kwin_windowview_config_SOURCES ../windowviewconfig.kcfgc)

kwin_add_effect_config(kwin_windowview_config ${kwin_windowview_config_SOURCES})
target_link_libraries(kwin_windowview_config
    KF5::ConfigWidgets
    KF5::CoreAddons
    KF5::GlobalAccel
    KF5::I18n
    KF5::XmlGui
    KWinEffectsInterface
)
