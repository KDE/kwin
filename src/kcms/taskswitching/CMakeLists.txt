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

# KNewStuff
install(FILES kwinswitcher.knsrc DESTINATION ${KDE_INSTALL_KNSRCDIR})
