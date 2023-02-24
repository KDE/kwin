# SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
#
# SPDX-License-Identifier: BSD-3-Clause

set(kwin_windowview_config_SOURCES windowvieweffectkcm.cpp)
ki18n_wrap_ui(kwin_windowview_config_SOURCES windowvieweffectkcm.ui)
kconfig_add_kcfg_files(kwin_windowview_config_SOURCES ../windowviewconfig.kcfgc)

kwin_add_effect_config(kwin_windowview_config ${kwin_windowview_config_SOURCES})
target_link_libraries(kwin_windowview_config
    KF6::ConfigWidgets
    KF6::CoreAddons
    KF6::GlobalAccel
    KF6::I18n
    KF6::XmlGui
    KWinEffectsInterface
)
