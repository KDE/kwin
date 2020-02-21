# KI18N Translation Domain for this library
add_definitions(-DTRANSLATION_DOMAIN=\"kcm_kwintabbox\")

include_directories(${KWin_SOURCE_DIR}/effects ${KWin_SOURCE_DIR}/tabbox ${KWin_SOURCE_DIR})

########### next target ###############

set(kcm_kwintabbox_PART_SRCS
    ${KWin_SOURCE_DIR}/tabbox/tabboxconfig.cpp
    layoutpreview.cpp
    main.cpp
    thumbnailitem.cpp
    kwintabboxconfigform.cpp
)

ki18n_wrap_ui(kcm_kwintabbox_PART_SRCS main.ui)
qt5_add_dbus_interface(kcm_kwintabbox_PART_SRCS ${KWin_SOURCE_DIR}/org.kde.kwin.Effects.xml kwin_effects_interface)

kconfig_add_kcfg_files(kcm_kwintabbox_PART_SRCS kwintabboxsettings.kcfgc kwinswitcheffectsettings.kcfgc kwinpluginssettings.kcfgc)
add_library(kcm_kwintabbox MODULE ${kcm_kwintabbox_PART_SRCS})

target_link_libraries(kcm_kwintabbox
    Qt5::Quick

    KF5::Completion
    KF5::GlobalAccel
    KF5::I18n
    KF5::KCMUtils
    KF5::NewStuff
    KF5::Package
    KF5::Service

    XCB::XCB

    kwin4_effect_builtins
)

install(TARGETS kcm_kwintabbox DESTINATION ${PLUGIN_INSTALL_DIR} )

########### install files ###############
install(FILES kwintabbox.desktop DESTINATION ${SERVICES_INSTALL_DIR})
install(FILES thumbnails/konqueror.png thumbnails/kmail.png thumbnails/systemsettings.png thumbnails/dolphin.png DESTINATION ${DATA_INSTALL_DIR}/kwin/kcm_kwintabbox)
install(FILES kwinswitcher.knsrc DESTINATION ${KDE_INSTALL_KNSRCDIR})
