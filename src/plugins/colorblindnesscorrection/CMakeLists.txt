# SPDX-FileCopyrightText: 2023 Fushan Wen <qydwhotmail@gmail.com>
# SPDX-License-Identifier: BSD-3-Clause

kwin_add_builtin_effect(colorblindnesscorrection
    colorblindnesscorrection.cpp
    colorblindnesscorrection.qrc
    main.cpp
)
kconfig_add_kcfg_files(colorblindnesscorrection colorblindnesscorrection_settings.kcfgc GENERATE_MOC)
target_link_libraries(colorblindnesscorrection PRIVATE
    kwin

    KF6::ConfigCore
    KF6::ConfigGui
)

# Config
if (NOT KWIN_BUILD_KCMS)
    return()
endif()

kcmutils_add_qml_kcm(kwin_colorblindnesscorrection_config SOURCES colorblindnesscorrection_config.cpp INSTALL_NAMESPACE "kwin/effects/configs" DISABLE_DESKTOP_FILE_GENERATION)
kcmutils_generate_module_data(kwin_colorblindnesscorrection_config
    MODULE_DATA_HEADER colorblindnesscorrection_settingsdata.h
    MODULE_DATA_CLASS_NAME ColorBlindnessCorrectionSettingsData
    SETTINGS_HEADERS colorblindnesscorrection_settings.h
    SETTINGS_CLASSES ColorBlindnessCorrectionSettings
)
kconfig_add_kcfg_files(kwin_colorblindnesscorrection_config colorblindnesscorrection_settings.kcfgc GENERATE_MOC)
target_link_libraries(kwin_colorblindnesscorrection_config
    KF6::ConfigCore
    KF6::KCMUtils
    KF6::KCMUtilsQuick
    KWinEffectsInterface
)
