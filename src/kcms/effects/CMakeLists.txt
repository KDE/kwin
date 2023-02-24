# KI18N Translation Domain for this library.
add_definitions(-DTRANSLATION_DOMAIN=\"kcm_kwin_effects\")

########### next target ###############

set(kcm_kwin_effects_PART_SRCS
    kcm.cpp
    effectsfilterproxymodel.cpp
    desktopeffectsdata.cpp
)

kcoreaddons_add_plugin(kcm_kwin_effects SOURCES ${kcm_kwin_effects_PART_SRCS} INSTALL_NAMESPACE "plasma/kcms/systemsettings")
kcmutils_generate_desktop_file(kcm_kwin_effects)

target_link_libraries(kcm_kwin_effects
    Qt::DBus

    KF6::KCMUtils
    KF6::I18n
    KF6::KCMUtils
    KF6::NewStuff
    KF6::QuickAddons
    KF6::XmlGui

    kcmkwincommon
)

########### install files ###############

install(FILES kwineffect.knsrc DESTINATION ${KDE_INSTALL_KNSRCDIR})
kpackage_install_package(package kcm_kwin_effects kcms)
