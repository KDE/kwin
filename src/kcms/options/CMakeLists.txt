########### next target ###############
# KI18N Translation Domain for this library
add_definitions(-DTRANSLATION_DOMAIN=\"kcmkwm\")

kcmutils_add_qml_kcm(kcm_kwinoptions
    SOURCES kwinoptions.cpp kwinoptionsdata.cpp
)

kcmutils_generate_module_data(kcm_kwinoptions
    MODULE_DATA_HEADER kwinoptionsdata.h
    MODULE_DATA_CLASS_NAME KWinOptionsData
    SETTINGS_HEADERS kwinoptions_settings.h kwinoptions_kdeglobals_settings.h
    SETTINGS_CLASSES KWinOptionsSettings KWinOptionsKDEGlobalsSettings
)

kconfig_add_kcfg_files(kcm_kwinoptions kwinoptions_settings.kcfgc GENERATE_MOC)
kconfig_add_kcfg_files(kcm_kwinoptions kwinoptions_kdeglobals_settings.kcfgc GENERATE_MOC)

target_link_libraries(kcm_kwinoptions PRIVATE
    Qt::DBus
    Qt::Quick

    KF6::ConfigGui

    KF6::KCMUtils
    KF6::KCMUtilsQuick
    KF6::I18n
    KF6::WindowSystem
    kwin
)

qt_add_dbus_interface(kcm_kwinoptions ${KWin_SOURCE_DIR}/src/org.kde.kwin.Effects.xml kwin_effects_interface)

