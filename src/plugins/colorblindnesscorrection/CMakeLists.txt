# SPDX-FileCopyrightText: 2023 Fushan Wen <qydwhotmail@gmail.com>
# SPDX-License-Identifier: BSD-3-Clause

kwin_add_builtin_effect(colorblindnesscorrection
    colorblindnesscorrection.cpp
    colorblindnesscorrection.qrc
    main.cpp
)
set(SETTINGS_HEADER "colorblindnesscorrection_settings_singleton")
set(GENERATE_MOC "false")
set(USE_SINGLETON "true")
set(KCFGFILE_ARGS "arg=\"true\"")
configure_file(colorblindnesscorrection_settings.kcfgc.in ${CMAKE_CURRENT_BINARY_DIR}/${SETTINGS_HEADER}.kcfgc @ONLY)
configure_file(colorblindnesscorrection_settings.kcfg.in ${CMAKE_CURRENT_BINARY_DIR}/${SETTINGS_HEADER}.kcfg @ONLY)
kconfig_add_kcfg_files(colorblindnesscorrection ${CMAKE_CURRENT_BINARY_DIR}/${SETTINGS_HEADER}.kcfgc)
target_link_libraries(colorblindnesscorrection PRIVATE
    kwin

    KF6::ConfigCore
    KF6::ConfigGui
)

# Config
if (NOT KWIN_BUILD_KCMS)
    return()
endif()

set(SETTINGS_HEADER "colorblindnesscorrection_settings")
kcmutils_add_qml_kcm(kwin_colorblindnesscorrection_config SOURCES colorblindnesscorrection_config.cpp INSTALL_NAMESPACE "kwin/effects/configs" DISABLE_DESKTOP_FILE_GENERATION)
kcmutils_generate_module_data(kwin_colorblindnesscorrection_config
    MODULE_DATA_HEADER colorblindnesscorrection_settingsdata.h
    MODULE_DATA_CLASS_NAME ColorBlindnessCorrectionSettingsData
    SETTINGS_HEADERS ${SETTINGS_HEADER}.h
    SETTINGS_CLASSES ColorBlindnessCorrectionSettings
)
set(GENERATE_MOC "true")
set(USE_SINGLETON "false")
set(KCFGFILE_ARGS "name=\"kwinrc\"")
configure_file(colorblindnesscorrection_settings.kcfgc.in ${CMAKE_CURRENT_BINARY_DIR}/${SETTINGS_HEADER}.kcfgc @ONLY)
configure_file(colorblindnesscorrection_settings.kcfg.in ${CMAKE_CURRENT_BINARY_DIR}/${SETTINGS_HEADER}.kcfg @ONLY)
kconfig_add_kcfg_files(kwin_colorblindnesscorrection_config ${CMAKE_CURRENT_BINARY_DIR}/colorblindnesscorrection_settings.kcfgc GENERATE_MOC)
target_link_libraries(kwin_colorblindnesscorrection_config
    KF6::ConfigCore
    KF6::KCMUtils
    KF6::KCMUtilsQuick
    KWinEffectsInterface
)
