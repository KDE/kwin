# KI18N Translation Domain for this library
add_definitions(-DTRANSLATION_DOMAIN=\"kcm_kwinrules\")
add_definitions(-DKCMRULES)

include_directories(../../)

set(kwinrules_SRCS
    ../../rulebooksettings.cpp
    ../../cursor.cpp
    ../../backends/x11/standalone/x11cursor.cpp
    ../../rules.cpp
    ../../placement.cpp
    ../../utils/common.cpp
    ../../virtualdesktopsdbustypes.cpp
    kwinsrc.cpp
    optionsmodel.cpp
    ruleitem.cpp
    rulesmodel.cpp
    rulebookmodel.cpp
)

kconfig_add_kcfg_files(kwinrules_SRCS ../../rulesettings.kcfgc)
kconfig_add_kcfg_files(kwinrules_SRCS ../../rulebooksettingsbase.kcfgc)

add_library(KWinRulesObjects STATIC ${kwinrules_SRCS})

set(kwin_kcm_rules_XCB_LIBS
    XCB::CURSOR
    XCB::XCB
    XCB::XFIXES
)

set(kcm_libs
    Qt::Quick
    Qt::QuickWidgets
    Qt::X11Extras

    KF5::KCMUtils
    KF5::I18n
    KF5::QuickAddons
    KF5::WindowSystem
    KF5::XmlGui
)

if (KWIN_BUILD_ACTIVITIES)
    set(kcm_libs ${kcm_libs} KF5::Activities)
endif()
target_link_libraries(KWinRulesObjects ${kcm_libs} ${kwin_kcm_rules_XCB_LIBS})

add_executable(kwin_rules_dialog main.cpp)
target_link_libraries(kwin_rules_dialog KWinRulesObjects)
install(TARGETS kwin_rules_dialog DESTINATION ${KDE_INSTALL_LIBEXECDIR})

add_library(kcm_kwinrules MODULE kcmrules.cpp)
target_link_libraries(kcm_kwinrules KWinRulesObjects)
kcoreaddons_desktop_to_json(kcm_kwinrules "kcm_kwinrules.desktop" SERVICE_TYPES kcmodule.desktop)

install(TARGETS kcm_kwinrules DESTINATION ${KDE_INSTALL_PLUGINDIR}/kcms)
install(FILES kcm_kwinrules.desktop DESTINATION ${KDE_INSTALL_KSERVICES5DIR})
kpackage_install_package(package kcm_kwinrules kcms)

install(FILES org.kde.kwin_rules_dialog.desktop DESTINATION ${KDE_INSTALL_APPDIR})
