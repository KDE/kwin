# KI18N Translation Domain for this library
add_definitions(-DTRANSLATION_DOMAIN=\"kcm_kwindecoration\")

add_subdirectory(declarative-plugin)

set(kcmkwindecoration_SRCS
    declarative-plugin/buttonsmodel.cpp
    decorationmodel.cpp
    kcm.cpp
    utils.cpp
)

kconfig_add_kcfg_files(kcmkwindecoration_SRCS kwindecorationsettings.kcfgc GENERATE_MOC)

add_library(kcm_kwindecoration MODULE ${kcmkwindecoration_SRCS})

target_link_libraries(kcm_kwindecoration
    KDecoration2::KDecoration
    KF5::I18n
    KF5::NewStuff
    KF5::QuickAddons
    Qt5::Quick
)

kcoreaddons_desktop_to_json(kcm_kwindecoration "kwindecoration.desktop" SERVICE_TYPES kcmodule.desktop)

# This desktop file is installed only for retrocompatibility with sycoca
install(FILES kwindecorationsettings.kcfg DESTINATION ${KDE_INSTALL_KCFGDIR})
install(FILES kwindecoration.desktop DESTINATION ${KDE_INSTALL_KSERVICES5DIR})
install(FILES window-decorations.knsrc DESTINATION ${KDE_INSTALL_KNSRCDIR})
install(TARGETS kcm_kwindecoration DESTINATION ${KDE_INSTALL_PLUGINDIR}/kcms)

kpackage_install_package(package kcm_kwindecoration kcms)
