# KI18N Translation Domain for this library
add_definitions(-DTRANSLATION_DOMAIN=\"kcm_kwintabbox\")

set(kcm_kwintabbox_PART_SRCS
    ${KWin_SOURCE_DIR}/src/tabbox/tabboxconfig.cpp
    main.cpp
    kwintabboxconfigform.cpp
    kwintabboxdata.cpp
    shortcutsettings.cpp
)

ki18n_wrap_ui(kcm_kwintabbox_PART_SRCS main.ui)
qt_add_dbus_interface(kcm_kwintabbox_PART_SRCS ${KWin_SOURCE_DIR}/src/org.kde.kwin.Effects.xml kwin_effects_interface)

kconfig_add_kcfg_files(kcm_kwintabbox_PART_SRCS kwintabboxsettings.kcfgc kwinswitcheffectsettings.kcfgc)
kcoreaddons_add_plugin(kcm_kwintabbox SOURCES ${kcm_kwintabbox_PART_SRCS} INSTALL_NAMESPACE "plasma/kcms/systemsettings_qwidgets")

kcmutils_generate_desktop_file(kcm_kwintabbox)
target_link_libraries(kcm_kwintabbox
    KF6::GlobalAccel
    KF6::I18n
    KF6::KCMUtils
    KF6::NewStuffWidgets
    KF6::Package
    KF6::Service
    KF6::XmlGui # For kkeysequencewidget
    KF6::WidgetsAddons
)
if (KWIN_BUILD_X11)
    target_link_libraries(kcm_kwintabbox
        XCB::XCB
    )
endif()

# TabBox preview helper
add_executable(kwin-tabbox-preview
    layoutpreview.cpp
    thumbnailitem.cpp
)
qt_add_resources(kwin-tabbox-preview "thumbnails"
    PREFIX "/kwin-tabbox-preview"
    BASE "thumbnails"
    FILES
        thumbnails/falkon.png
        thumbnails/kmail.png
        thumbnails/systemsettings.png
        thumbnails/dolphin.png
        thumbnails/desktop.png
)
target_link_libraries(kwin-tabbox-preview
    Qt::Quick
    KF6::I18n
)
install(TARGETS kwin-tabbox-preview DESTINATION ${KDE_INSTALL_LIBEXECDIR})

# KNewStuff
install(FILES kwinswitcher.knsrc DESTINATION ${KDE_INSTALL_KNSRCDIR})
