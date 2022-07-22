# KI18N Translation Domain for this library.
add_definitions(-DTRANSLATION_DOMAIN=\"kcm_kwin_virtualdesktops\")

########### next target ###############

set(kcm_kwin_virtualdesktops_PART_SRCS
    ../../virtualdesktopsdbustypes.cpp
    animationsmodel.cpp
    desktopsmodel.cpp
    virtualdesktops.cpp
    virtualdesktopsdata.cpp
)

kconfig_add_kcfg_files(kcm_kwin_virtualdesktops_PART_SRCS virtualdesktopssettings.kcfgc GENERATE_MOC)

kcoreaddons_add_plugin(kcm_kwin_virtualdesktops SOURCES ${kcm_kwin_virtualdesktops_PART_SRCS} INSTALL_NAMESPACE "plasma/kcms/systemsettings")
kcmutils_generate_desktop_file(kcm_kwin_virtualdesktops)

target_link_libraries(kcm_kwin_virtualdesktops
    Qt::DBus

    KF5::I18n
    KF5::KCMUtils
    KF5::QuickAddons
    KF5::XmlGui

    kcmkwincommon
)

########### install files ###############

install(FILES virtualdesktopssettings.kcfg DESTINATION ${KDE_INSTALL_KCFGDIR})
kpackage_install_package(package kcm_kwin_virtualdesktops kcms)
