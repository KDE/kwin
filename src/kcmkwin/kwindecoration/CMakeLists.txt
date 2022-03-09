# KI18N Translation Domain for this library
add_definitions(-DTRANSLATION_DOMAIN=\"kcm_kwindecoration\")

add_subdirectory(declarative-plugin)

set(kcmkwindecoration_SRCS
    declarative-plugin/buttonsmodel.cpp
    decorationmodel.cpp
    kcm.cpp
    utils.cpp
)

kcmutils_generate_module_data(
    kcmkwindecoration_SRCS
    MODULE_DATA_HEADER kwindecorationdata.h
    MODULE_DATA_CLASS_NAME KWinDecorationData
    SETTINGS_HEADERS kwindecorationsettings.h
    SETTINGS_CLASSES KWinDecorationSettings
)

kconfig_add_kcfg_files(kcmkwindecoration_SRCS kwindecorationsettings.kcfgc GENERATE_MOC)

add_library(kcm_kwindecoration MODULE ${kcmkwindecoration_SRCS})

target_link_libraries(kcm_kwindecoration
    KDecoration2::KDecoration
    KF5::I18n
    KF5::KCMUtils
    KF5::NewStuff
    KF5::QuickAddons
    Qt::Quick
)

kcoreaddons_desktop_to_json(kcm_kwindecoration "kwindecoration.desktop" SERVICE_TYPES kcmodule.desktop)

set(kwin-applywindowdecoration_SRCS
    kwin-applywindowdecoration.cpp
    decorationmodel.cpp
    utils.cpp
)

kconfig_add_kcfg_files(kwin-applywindowdecoration_SRCS kwindecorationsettings.kcfgc GENERATE_MOC)
add_executable(kwin-applywindowdecoration ${kwin-applywindowdecoration_SRCS})

target_link_libraries(kwin-applywindowdecoration
    KDecoration2::KDecoration
    Qt::DBus
    KF5::I18n
    KF5::KCMUtils
)

configure_file(window-decorations.knsrc.cmake ${CMAKE_CURRENT_BINARY_DIR}/window-decorations.knsrc)

# This desktop file is installed only for retrocompatibility with sycoca
install(FILES kwindecorationsettings.kcfg DESTINATION ${KDE_INSTALL_KCFGDIR})
install(FILES kwindecoration.desktop DESTINATION ${KDE_INSTALL_KSERVICESDIR})
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/window-decorations.knsrc DESTINATION ${KDE_INSTALL_KNSRCDIR})
install(TARGETS kcm_kwindecoration DESTINATION ${KDE_INSTALL_PLUGINDIR}/kcms)
install(TARGETS kwin-applywindowdecoration DESTINATION ${KDE_INSTALL_LIBEXECDIR})

kpackage_install_package(package kcm_kwindecoration kcms)
