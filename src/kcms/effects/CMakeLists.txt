# KI18N Translation Domain for this library.
add_definitions(-DTRANSLATION_DOMAIN=\"kcm_kwin_effects\")

########### next target ###############

set(kcm_kwin_effects_PART_SRCS
    kcm.cpp
    effectsfilterproxymodel.cpp
    effectskcmmodel.cpp
    desktopeffectsdata.cpp
)

kcmutils_add_qml_kcm(kcm_kwin_effects SOURCES ${kcm_kwin_effects_PART_SRCS})

target_link_libraries(kcm_kwin_effects PRIVATE
    Qt::DBus
    Qt::Quick

    KF6::KCMUtils
    KF6::I18n
    KF6::KCMUtils
    KF6::KCMUtilsQuick
    KF6::XmlGui

    kcmkwincommon
)

install(FILES kwineffect.knsrc DESTINATION ${KDE_INSTALL_KNSRCDIR})
