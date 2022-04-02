# KI18N Translation Domain for this library
add_definitions(-DTRANSLATION_DOMAIN=\"kcm-kwin-scripts\")

include_directories(${CMAKE_CURRENT_BINARY_DIR})

set(kcm_SRCS
    module.cpp
    kwinscriptsdata.cpp
)

add_library(kcm_kwin_scripts MODULE ${kcm_SRCS})
kcoreaddons_desktop_to_json(kcm_kwin_scripts kwinscripts.desktop)

target_link_libraries(kcm_kwin_scripts
    Qt::DBus

    KF5::I18n
    KF5::KCMUtilsCore
    KF5::KCMUtils
    KF5::NewStuffWidgets
    KF5::Package
    KF5::QuickAddons
    KF5::ItemViews
)

kpackage_install_package(package kcm_kwin_scripts kcms)
install(TARGETS kcm_kwin_scripts DESTINATION ${KDE_INSTALL_PLUGINDIR}/kcms)
install(FILES kwinscripts.desktop DESTINATION ${KDE_INSTALL_KSERVICESDIR})
install(FILES kwinscripts.knsrc DESTINATION ${KDE_INSTALL_KNSRCDIR})
