add_definitions(-DTRANSLATION_DOMAIN=\"kcm_kwin_taskswitching\")

set(kcm_kwin_taskswitching_PART_SRCS
    ${CMAKE_SOURCE_DIR}/src/tabbox/tabboxconfig.cpp
    kcm.cpp
    taskswitchingdata.cpp
    tabboxlayoutsmodel.cpp
)

kcmutils_add_qml_kcm(kcm_kwin_taskswitching SOURCES ${kcm_kwin_taskswitching_PART_SRCS})

kconfig_target_kcfg_file(kcm_kwin_taskswitching
    FILE tabboxsettings.kcfg
    CLASS_NAME TabBoxSettings
    NAME_SPACE KWin
    INCLUDE_FILES
        tabbox/tabboxconfig.h
    MUTATORS
    DEFAULT_VALUE_GETTERS
    GENERATE_PROPERTIES
    PARENT_IN_CONSTRUCTOR
    GENERATE_MOC
)

kconfig_target_kcfg_file(kcm_kwin_taskswitching
    FILE overviewsettings.kcfg
    CLASS_NAME OverviewSettings
    NAME_SPACE KWin
    MUTATORS
    DEFAULT_VALUE_GETTERS
    GENERATE_PROPERTIES
    PARENT_IN_CONSTRUCTOR
    GENERATE_MOC
)

target_link_libraries(kcm_kwin_taskswitching PRIVATE
    Qt::DBus
    Qt::Quick
    KF6::ConfigGui
    KF6::I18n
    KF6::KCMUtilsQuick
    KF6::GlobalAccel
    KF6::I18n
    KF6::Package
    KF6::XmlGui # For kkeysequencewidget
)

# KNewStuff
install(FILES kwinswitcher.knsrc DESTINATION ${KDE_INSTALL_KNSRCDIR})
