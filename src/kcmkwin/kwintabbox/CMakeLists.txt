# KI18N Translation Domain for this library
add_definitions(-DTRANSLATION_DOMAIN=\"kcm_kwintabbox\")

include_directories(${KWin_SOURCE_DIR}/src/effects ${KWin_SOURCE_DIR}/src/tabbox ${KWin_SOURCE_DIR})

########### next target ###############

set(kcm_kwintabbox_PART_SRCS
    ${KWin_SOURCE_DIR}/src/tabbox/tabboxconfig.cpp
    layoutpreview.cpp
    main.cpp
    thumbnailitem.cpp
    kwintabboxconfigform.cpp
    kwintabboxdata.cpp
)

ki18n_wrap_ui(kcm_kwintabbox_PART_SRCS main.ui)
qt_add_dbus_interface(kcm_kwintabbox_PART_SRCS ${KWin_SOURCE_DIR}/src/org.kde.kwin.Effects.xml kwin_effects_interface)

kconfig_add_kcfg_files(kcm_kwintabbox_PART_SRCS kwintabboxsettings.kcfgc kwinswitcheffectsettings.kcfgc kwinpluginssettings.kcfgc)
kcoreaddons_add_plugin(kcm_kwintabbox SOURCES ${kcm_kwintabbox_PART_SRCS} INSTALL_NAMESPACE "plasma/kcms/systemsettings_qwidgets")

kcmutils_generate_desktop_file(kcm_kwintabbox)
target_link_libraries(kcm_kwintabbox
    Qt::Quick

    KF5::GlobalAccel
    KF5::I18n
    KF5::KCMUtils
    KF5::NewStuffWidgets
    KF5::Package
    KF5::Service

    XCB::XCB
)

########### install files ###############
install(FILES thumbnails/konqueror.png
              thumbnails/kmail.png
              thumbnails/systemsettings.png
              thumbnails/dolphin.png
              thumbnails/desktop.png
        DESTINATION ${KDE_INSTALL_DATADIR}/kwin/kcm_kwintabbox)
install(FILES kwinswitcher.knsrc DESTINATION ${KDE_INSTALL_KNSRCDIR})
