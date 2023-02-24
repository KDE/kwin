# KI18N Translation Domain for this library
add_definitions(-DTRANSLATION_DOMAIN=\"kcm_kwin_scripts\")

kcoreaddons_add_plugin(kcm_kwin_scripts INSTALL_NAMESPACE plasma/kcms/systemsettings)

kcmutils_generate_desktop_file(kcm_kwin_scripts)
target_sources(kcm_kwin_scripts PRIVATE
    module.cpp
    kwinscriptsdata.cpp
)

target_link_libraries(kcm_kwin_scripts
    Qt::DBus

    KF6::I18n
    KF6::KCMUtilsCore
    KF6::KCMUtils
    KF6::NewStuffWidgets
    KF6::Package
    KF6::QuickAddons
    KF6::ItemViews
)

kpackage_install_package(package kcm_kwin_scripts kcms)
install(FILES kwinscripts.knsrc DESTINATION ${KDE_INSTALL_KNSRCDIR})
