add_definitions(-DTRANSLATION_DOMAIN=\"kcm_kwin_taskswitching\")

set(kcm_kwin_taskswitching_PART_SRCS
    ${KWin_SOURCE_DIR}/src/tabbox/tabboxconfig.cpp
    kcm.cpp
    taskswitchingdata.cpp
    tabboxlayoutsmodel.cpp
)

kconfig_add_kcfg_files(kcm_kwin_taskswitching_PART_SRCS tabboxsettings.kcfgc overviewsettings.kcfgc GENERATE_MOC)

kcmutils_add_qml_kcm(kcm_kwin_taskswitching SOURCES ${kcm_kwin_taskswitching_PART_SRCS})

# TODO: Cleanup link libraries
target_link_libraries(kcm_kwin_taskswitching PRIVATE
    Qt::DBus
    Qt::Quick
    KF6::ConfigGui
    KF6::KCMUtils
    KF6::I18n
    KF6::KCMUtilsQuick
    KF6::Service

    KF6::GlobalAccel
    KF6::I18n
    KF6::KCMUtils
    KF6::NewStuffWidgets
    KF6::Package
    KF6::Service
    KF6::XmlGui # For kkeysequencewidget
    KF6::WidgetsAddons
)

# TabBox preview helper
add_executable(kwin-tabbox-preview
    preview/layoutpreview.cpp
    preview/thumbnailitem.cpp
)
qt_add_resources(kwin-tabbox-preview "thumbnails"
    PREFIX "/kwin-tabbox-preview"
    BASE "preview/thumbnails"
    FILES
        preview/thumbnails/falkon.png
        preview/thumbnails/kmail.png
        preview/thumbnails/systemsettings.png
        preview/thumbnails/dolphin.png
        preview/thumbnails/desktop.png
)
target_link_libraries(kwin-tabbox-preview
    Qt::Quick
    KF6::I18n
)
install(TARGETS kwin-tabbox-preview DESTINATION ${KDE_INSTALL_LIBEXECDIR})

# KNewStuff
install(FILES kwinswitcher.knsrc DESTINATION ${KDE_INSTALL_KNSRCDIR})
