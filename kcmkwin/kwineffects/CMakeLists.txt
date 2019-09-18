include(ECMQMLModules)
ecm_find_qmlmodule(org.kde.plasma.core 2.0)

# KI18N Translation Domain for this library.
add_definitions(-DTRANSLATION_DOMAIN=\"kcm_kwin_effects\")

########### next target ###############

set(kcm_kwin_effects_PART_SRCS
    kcm.cpp
    effectsfilterproxymodel.cpp
)

add_library(kcm_kwin_effects MODULE ${kcm_kwin_effects_PART_SRCS})

target_link_libraries(kcm_kwin_effects
    Qt5::DBus

    KF5::I18n
    KF5::KCMUtils
    KF5::NewStuff
    KF5::QuickAddons
    KF5::XmlGui

    kcmkwincommon
)

kcoreaddons_desktop_to_json(kcm_kwin_effects "kcm_kwin_effects.desktop")

########### install files ###############

install(TARGETS kcm_kwin_effects DESTINATION ${KDE_INSTALL_PLUGINDIR}/kcms)
install(FILES kcm_kwin_effects.desktop DESTINATION ${KDE_INSTALL_KSERVICES5DIR})
install(FILES kwineffect.knsrc DESTINATION ${KDE_INSTALL_KNSRCDIR})
kpackage_install_package(package kcm_kwin_effects kcms)
