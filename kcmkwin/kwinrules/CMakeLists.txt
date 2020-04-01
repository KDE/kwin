# KI18N Translation Domain for this library
add_definitions(-DTRANSLATION_DOMAIN=\"kcmkwinrules\")
add_definitions(-DKCMRULES)

include_directories(../../)
set(kwinrules_SRCS  ../../rulebooksettings.cpp
                    ../../cursor.cpp
                    ../../plugins/platforms/x11/standalone/x11cursor.cpp
                    ../../rules.cpp
                    ../../placement.cpp
                    ../../utils.cpp
                    yesnobox.cpp
                    ruleswidget.cpp
                    ruleslist.cpp
                    kwinsrc.cpp
                    detectwidget.cpp
                    )

ki18n_wrap_ui(kwinrules_SRCS ruleslist.ui detectwidget.ui editshortcut.ui ruleswidgetbase.ui)

kconfig_add_kcfg_files(kwinrules_SRCS ../../rulesettings.kcfgc)
kconfig_add_kcfg_files(kwinrules_SRCS ../../rulebooksettingsbase.kcfgc)

add_library(KWinRulesObjects OBJECT ${kwinrules_SRCS})

set(kwin_kcm_rules_XCB_LIBS
    XCB::CURSOR
    XCB::XCB
    XCB::XFIXES
)

set(kcm_libs
    Qt5::Concurrent
    Qt5::X11Extras

    KF5::Completion
    KF5::ConfigWidgets
    KF5::I18n
    KF5::Service
    KF5::WindowSystem
    KF5::XmlGui
)

if (KWIN_BUILD_ACTIVITIES)
    set(kcm_libs ${kcm_libs} KF5::Activities)
endif()
target_link_libraries(KWinRulesObjects ${kcm_libs} ${kwin_kcm_rules_XCB_LIBS})

add_executable(kwin_rules_dialog main.cpp)
target_link_libraries(kwin_rules_dialog KWinRulesObjects)
install(TARGETS kwin_rules_dialog DESTINATION ${LIBEXEC_INSTALL_DIR})

add_library(kcm_kwinrules MODULE kcm.cpp)
target_link_libraries(kcm_kwinrules KWinRulesObjects)
install(TARGETS kcm_kwinrules DESTINATION ${PLUGIN_INSTALL_DIR})

########### install files ###############

install(FILES kwinrules.desktop DESTINATION ${SERVICES_INSTALL_DIR})
