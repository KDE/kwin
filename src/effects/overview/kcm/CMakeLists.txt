# SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
#
# SPDX-License-Identifier: BSD-3-Clause

set(kwin_overview_config_SOURCES overvieweffectkcm.cpp)
ki18n_wrap_ui(kwin_overview_config_SOURCES overvieweffectkcm.ui)
kconfig_add_kcfg_files(kwin_overview_config_SOURCES ../overviewconfig.kcfgc)

add_library(kwin_overview_config MODULE ${kwin_overview_config_SOURCES})
target_link_libraries(kwin_overview_config
    KF5::Completion
    KF5::ConfigWidgets
    KF5::GlobalAccel
    KF5::I18n
    KF5::XmlGui
    KWinEffectsInterface
)

kcoreaddons_desktop_to_json(kwin_overview_config overvieweffectkcm.desktop SERVICE_TYPES kcmodule.desktop)
install(TARGETS kwin_overview_config DESTINATION ${KDE_INSTALL_PLUGINDIR}/kwin/effects/configs)
