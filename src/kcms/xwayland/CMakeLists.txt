#SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalzez <aleixpol@kde.org>
#SPDX-License-Identifier: BSD-3-Clause

add_definitions(-DTRANSLATION_DOMAIN=\"kcm_kwinxwayland\")

kcmutils_generate_module_data(
    kcm_kwinxwayland_PART_SRCS
    MODULE_DATA_HEADER kwinxwaylanddata.h
    MODULE_DATA_CLASS_NAME KWinXwaylandData
    SETTINGS_HEADERS kwinxwaylandsettings.h
    SETTINGS_CLASSES KWinXwaylandSettings
)

kconfig_add_kcfg_files(kcm_kwinxwayland_PART_SRCS kwinxwaylandsettings.kcfgc GENERATE_MOC)
kcmutils_add_qml_kcm(kcm_kwinxwayland SOURCES kcmkwinxwayland.cpp ${kcm_kwinxwayland_PART_SRCS})

target_link_libraries(kcm_kwinxwayland PRIVATE
    KF6::I18n
    KF6::KCMUtils
    KF6::KCMUtilsQuick
    Wayland::Client
)
