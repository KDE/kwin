# KI18N Translation Domain for this library
add_definitions(-DTRANSLATION_DOMAIN=\"kcm-kwin-scripts\")

kcoreaddons_add_plugin(kcm_kwin_scripts INSTALL_NAMESPACE plasma/kcms/systemsettings)

target_sources(kcm_kwin_scripts PRIVATE
    module.cpp
    kwinscriptsdata.cpp
)

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
install(FILES kwinscripts.desktop DESTINATION ${KDE_INSTALL_APPDIR})
install(FILES kwinscripts.knsrc DESTINATION ${KDE_INSTALL_KNSRCDIR})
