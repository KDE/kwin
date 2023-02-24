# SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
#
# SPDX-License-Identifier: BSD-3-Clause

set(kwin_overview_config_SOURCES overvieweffectkcm.cpp)
ki18n_wrap_ui(kwin_overview_config_SOURCES overvieweffectkcm.ui)
kconfig_add_kcfg_files(kwin_overview_config_SOURCES ../overviewconfig.kcfgc)

kwin_add_effect_config(kwin_overview_config ${kwin_overview_config_SOURCES})
target_link_libraries(kwin_overview_config
    KF6::ConfigWidgets
    KF6::CoreAddons
    KF6::GlobalAccel
    KF6::I18n
    KF6::XmlGui
    KWinEffectsInterface
)
